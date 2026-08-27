"""Shared filesystem and shell policy for QCopilots services."""

from __future__ import annotations

import json
import os
import re
import stat
from dataclasses import dataclass
from pathlib import Path, PureWindowsPath
from typing import Any
from urllib.parse import unquote, urlsplit


SECURITY_POLICY_ENV = "QCOPILOTS_SECURITY_POLICY"
COMPATIBLE_MODE = "compatible"
FORMAL_RESTRICTED_MODE = "formal_restricted"

_WINDOWS_DEVICE_PATH_PREFIXES = (
    "\\\\?\\",
    "\\\\.\\",
    "\\??\\",
    "//?/",
    "//./",
    "/??/",
)
_WINDOWS_RESERVED_DEVICE_RE = re.compile(
    r"^(?:CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|"
    r"COM(?:[1-9]|\u00b9|\u00b2|\u00b3)|"
    r"LPT(?:[1-9]|\u00b9|\u00b2|\u00b3))$",
    re.IGNORECASE,
)
_PROVIDER_URI_FIELD_RE = re.compile(
    r"(?i)(?:^|[&\s]+)([a-z][a-z0-9_.-]*)\s*=\s*"
    r"(?:'([^']*)'|\"([^\"]*)\"|([^&\s]*))"
)
_PROVIDER_LOCAL_PATH_FIELDS = frozenset({"path", "file", "filename"})
_PROVIDER_CONNECTION_FIELDS = frozenset(
    {"host", "service", "port", "user", "username", "password", "sslmode", "authcfg"}
)
_REMOTE_DATABASE_PROVIDERS = frozenset(
    {"db2", "hana", "mssql", "mysql", "oracle", "postgres", "postgresraster"}
)
_LOCAL_DATABASE_SUFFIXES = frozenset(
    {".db", ".db3", ".gpkg", ".mdb", ".sqlite", ".sqlite3", ".spatialite"}
)
_LOCAL_PROVIDER_DATASOURCE_RE = re.compile(
    r"^(?:CSV|FILEGDB|GPKG|OPENFILEGDB|PGEO|SQLITE)\s*:(.*)$",
    re.IGNORECASE | re.DOTALL,
)


@dataclass(frozen=True)
class FilesystemPolicy:
    """Validated local path, shell and network policy shared by every service."""

    mode: str = COMPATIBLE_MODE
    shell_enabled: bool = True
    shell_executables: tuple[str, ...] = ()
    network_enabled: bool = True
    network_allowed_origins: tuple[str, ...] = ()

    @property
    def restricted(self) -> bool:
        return self.mode == FORMAL_RESTRICTED_MODE

    def resolve_path(
        self,
        value: str | Path,
        *,
        access: str,
        base: str | Path | None = None,
    ) -> Path:
        """Resolve an unrestricted normal local path for the requested access."""

        if access not in {"read", "write", "shell"}:
            raise ValueError(f"Unsupported filesystem access mode: {access}")
        raw = str(value).strip()
        if not raw:
            raise ValueError("Path must not be empty")
        path_safety_required = access in {"read", "write"} or self.restricted
        if path_safety_required:
            _reject_restricted_path_syntax(raw)

        candidate = Path(raw).expanduser()
        if not candidate.is_absolute():
            if base is not None:
                base_raw = str(base).strip()
                if path_safety_required:
                    _reject_restricted_path_syntax(base_raw)
                base_path = Path(base_raw).expanduser()
                if self.restricted and not base_path.is_absolute():
                    raise PermissionError(
                        "Formal restricted mode requires an absolute base path"
                    )
                candidate = base_path / candidate
            else:
                candidate = Path.cwd() / candidate
        candidate = Path(os.path.abspath(candidate))

        if path_safety_required:
            _reject_reparse_components(candidate)
        resolved = candidate.resolve(strict=False)
        if path_safety_required:
            # Revalidate the canonical spelling because a platform resolver may
            # expose a device or network form which was not visible in the input.
            _reject_restricted_path_syntax(str(resolved))
            _reject_remote_windows_drive(resolved)
        if access == "write" or (self.restricted and access == "shell"):
            _reject_multiply_linked_regular_file(resolved)
        return resolved

    def validate_network_url(self, value: str) -> str:
        """Validate a remote URL against the formal loopback allowlist."""

        if not self.restricted:
            return value
        if not self.network_enabled:
            raise PermissionError(
                "Formal restricted mode does not permit network sources"
            )
        origin = _request_network_origin(value)
        if origin not in self.network_allowed_origins:
            raise PermissionError(
                f"Network source origin is outside the configured allowlist: {origin}"
            )
        return value

    def resolve_file_uri(self, value: str, *, access: str) -> Path:
        """Resolve a local file URI through the normal filesystem policy."""

        parsed = urlsplit(str(value).strip())
        if parsed.scheme.casefold() != "file":
            raise PermissionError("The filesystem policy expected a local file URI")
        if parsed.username is not None or parsed.password is not None:
            raise PermissionError(
                "The filesystem policy does not permit credentials in file URIs"
            )
        if parsed.query or parsed.fragment:
            raise PermissionError(
                "The filesystem policy does not permit query or fragment data in file URIs"
            )
        if parsed.netloc.casefold() not in {"", "localhost"}:
            raise PermissionError(
                "The filesystem policy does not permit remote file URI authorities"
            )
        path = unquote(parsed.path)
        if re.match(r"^/[A-Za-z]:/", path):
            path = path[1:]
        if not path:
            raise ValueError("A file URI must identify a local path")
        return self.resolve_path(path, access=access)

    def validate_provider_local_paths(
        self,
        value: str,
        *,
        access: str,
        provider: str = "",
    ) -> None:
        """Validate local path fields embedded in an otherwise opaque provider URI."""

        decoded = _decode_provider_component(str(value))
        datasource_match = _LOCAL_PROVIDER_DATASOURCE_RE.match(decoded.strip())
        if datasource_match is not None:
            datasource_path = datasource_match.group(1).strip().strip("\"'")
            if datasource_path and not datasource_path.casefold().startswith("/vsi"):
                if datasource_path.casefold().startswith("file:"):
                    self.resolve_file_uri(datasource_path, access=access)
                else:
                    self.resolve_path(datasource_path, access=access)
        matches = list(_PROVIDER_URI_FIELD_RE.finditer(decoded))
        keys = {match.group(1).strip().casefold() for match in matches}
        has_connection_fields = bool(
            keys & _PROVIDER_CONNECTION_FIELDS
            or str(provider).strip().casefold() in _REMOTE_DATABASE_PROVIDERS
        )
        for match in matches:
            key = match.group(1).strip().casefold()
            field_value = next(
                (item for item in match.groups()[1:] if item is not None), ""
            ).strip()
            if not field_value or not _provider_field_is_local_path(
                key,
                field_value,
                has_connection_fields=has_connection_fields,
            ):
                continue
            if field_value.casefold().startswith("file:"):
                self.resolve_file_uri(field_value, access=access)
            else:
                self.resolve_path(field_value, access=access)

    def validate_provider_options_local_paths(
        self,
        value: dict[str, Any],
        *,
        access: str,
        provider: str = "",
    ) -> None:
        """Validate local paths supplied as structured provider URI options."""

        fields: list[tuple[str, str]] = []
        for raw_key, raw_value in value.items():
            key = _decode_provider_component(str(raw_key)).strip().casefold()
            items = raw_value if isinstance(raw_value, (list, tuple)) else [raw_value]
            for item in items:
                if not isinstance(item, str):
                    continue
                field_value = _decode_provider_component(item).strip()
                if field_value:
                    fields.append((key, field_value))
        keys = {key for key, _field_value in fields}
        has_connection_fields = bool(
            keys & _PROVIDER_CONNECTION_FIELDS
            or str(provider).strip().casefold() in _REMOTE_DATABASE_PROVIDERS
        )
        for key, field_value in fields:
            if not _provider_field_is_local_path(
                key,
                field_value,
                has_connection_fields=has_connection_fields,
            ):
                continue
            if field_value.casefold().startswith("file:"):
                self.resolve_file_uri(field_value, access=access)
            else:
                self.resolve_path(field_value, access=access)

    def to_config(self) -> dict[str, Any]:
        return {
            "mode": self.mode,
            "shell": {
                "enabled": self.shell_enabled,
                "executables": list(self.shell_executables),
            },
            "network": {
                "enabled": self.network_enabled,
                "allowed_origins": list(self.network_allowed_origins),
            },
        }

    def to_environment(self) -> dict[str, str]:
        return {
            SECURITY_POLICY_ENV: json.dumps(
                self.to_config(),
                ensure_ascii=False,
                separators=(",", ":"),
            )
        }


def filesystem_policy_from_config(value: Any) -> FilesystemPolicy:
    """Validate a manager security_policy object."""

    if value is None:
        return FilesystemPolicy()
    if not isinstance(value, dict):
        raise ValueError("security_policy must be an object")
    obsolete = {"read_roots", "write_roots"}.intersection(value)
    if obsolete:
        raise ValueError(
            "security_policy contains obsolete fields that are no longer supported: "
            + ", ".join(sorted(obsolete))
        )
    unexpected = set(value) - {
        "mode",
        "shell",
        "network",
    }
    if unexpected:
        raise ValueError(
            "security_policy contains unsupported fields: "
            + ", ".join(sorted(str(item) for item in unexpected))
        )
    missing = {"mode", "shell", "network"} - set(value)
    if missing:
        raise ValueError(
            "security_policy is missing required fields: "
            + ", ".join(sorted(missing))
        )
    mode = value["mode"]
    if mode not in {COMPATIBLE_MODE, FORMAL_RESTRICTED_MODE}:
        raise ValueError(
            "security_policy.mode must be compatible or formal_restricted"
        )
    shell = value["shell"]
    if not isinstance(shell, dict):
        raise ValueError("security_policy.shell must be an object")
    unexpected_shell = set(shell) - {"enabled", "executables"}
    if unexpected_shell:
        raise ValueError(
            "security_policy.shell contains unsupported fields: "
            + ", ".join(sorted(str(item) for item in unexpected_shell))
        )
    missing_shell = {"enabled", "executables"} - set(shell)
    if missing_shell:
        raise ValueError(
            "security_policy.shell is missing required fields: "
            + ", ".join(sorted(missing_shell))
        )
    shell_enabled = shell["enabled"]
    if not isinstance(shell_enabled, bool):
        raise ValueError("security_policy.shell.enabled must be a boolean")
    executables = shell["executables"]
    if not isinstance(executables, list) or any(
        not isinstance(item, str) or not item.strip() for item in executables
    ):
        raise ValueError(
            "security_policy.shell.executables must contain non-empty strings"
        )
    network = value["network"]
    if not isinstance(network, dict):
        raise ValueError("security_policy.network must be an object")
    unexpected_network = set(network) - {"enabled", "allowed_origins"}
    if unexpected_network:
        raise ValueError(
            "security_policy.network contains unsupported fields: "
            + ", ".join(sorted(str(item) for item in unexpected_network))
        )
    missing_network = {"enabled", "allowed_origins"} - set(network)
    if missing_network:
        raise ValueError(
            "security_policy.network is missing required fields: "
            + ", ".join(sorted(missing_network))
        )
    network_enabled = network["enabled"]
    if not isinstance(network_enabled, bool):
        raise ValueError("security_policy.network.enabled must be a boolean")
    origins = network["allowed_origins"]
    if not isinstance(origins, list) or any(
        not isinstance(item, str) or not item.strip() for item in origins
    ):
        raise ValueError(
            "security_policy.network.allowed_origins must contain non-empty strings"
        )
    allowed_origins = tuple(
        dict.fromkeys(_validated_network_origin(item) for item in origins)
    )
    if network_enabled and mode == FORMAL_RESTRICTED_MODE and not allowed_origins:
        raise ValueError(
            "Formal restricted network access requires at least one allowed origin"
        )
    return FilesystemPolicy(
        mode=mode,
        shell_enabled=shell_enabled,
        shell_executables=tuple(dict.fromkeys(item.strip() for item in executables)),
        network_enabled=network_enabled,
        network_allowed_origins=allowed_origins,
    )


def filesystem_policy_from_environment() -> FilesystemPolicy:
    raw = os.environ.get(SECURITY_POLICY_ENV)
    if raw is None or not raw.strip():
        return FilesystemPolicy()
    try:
        value = json.loads(raw)
    except json.JSONDecodeError as err:
        raise ValueError(f"Invalid {SECURITY_POLICY_ENV} JSON") from err
    return filesystem_policy_from_config(value)


def _looks_like_uri(value: str) -> bool:
    if re.match(r"^[A-Za-z]:[\\/]", value):
        return False
    return bool(re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", value)) or "://" in value


def _decode_provider_component(value: str) -> str:
    decoded = value
    for _index in range(4):
        next_value = unquote(decoded)
        if next_value == decoded:
            break
        decoded = next_value
    return decoded


def _provider_field_is_local_path(
    key: str,
    value: str,
    *,
    has_connection_fields: bool,
) -> bool:
    if key in _PROVIDER_LOCAL_PATH_FIELDS:
        return True
    if key not in {"dbname", "url"}:
        return False
    lowered = value.casefold()
    if lowered.startswith("file:"):
        return True
    if key == "url":
        windows_path = PureWindowsPath(value)
        if lowered.startswith("/vsi"):
            return False
        if windows_path.drive:
            return True
        if _looks_like_uri(value):
            return False
        return bool(
            "\0" in value
            or value.startswith(("/", "\\", "~"))
        )
    windows_path = PureWindowsPath(value)
    obvious_local_path = bool(
        "\0" in value
        or windows_path.is_absolute()
        or "/" in value
        or "\\" in value
        or value.startswith((".", "~"))
        or _contains_windows_reserved_device_name(value)
        or (
            os.name == "nt"
            and bool(
                re.search(
                    r"\.(?:db|db3|gpkg|mdb|sqlite|sqlite3|spatialite):",
                    value,
                    re.IGNORECASE,
                )
            )
        )
    )
    if obvious_local_path:
        return True
    if has_connection_fields:
        return False
    return bool(
        (os.name == "nt" and ":" in value)
        or windows_path.suffix.casefold() in _LOCAL_DATABASE_SUFFIXES
    )


def _reject_restricted_path_syntax(value: str) -> None:
    """Reject remote or device-like path forms without touching the filesystem."""

    if "\0" in value:
        raise PermissionError(
            "The filesystem policy does not permit NUL characters in paths"
        )
    normalized = value.replace("/", "\\")
    device_prefixes = tuple(
        item.replace("/", "\\") for item in _WINDOWS_DEVICE_PATH_PREFIXES
    )
    if normalized.startswith(device_prefixes):
        raise PermissionError(
            "The filesystem policy does not permit Windows device paths"
        )
    if normalized.startswith("\\\\"):
        raise PermissionError("The filesystem policy does not permit UNC paths")

    windows_path = PureWindowsPath(value)
    if windows_path.drive and not windows_path.is_absolute():
        raise PermissionError(
            "The filesystem policy does not permit drive-relative paths"
        )
    if windows_path.drive and os.name != "nt":
        raise PermissionError(
            "The filesystem policy cannot validate Windows paths on this platform"
        )
    if _looks_like_uri(value):
        raise PermissionError(
            "The filesystem policy does not permit URI path values"
        )
    if os.name == "nt" and _contains_windows_reserved_device_name(value):
        raise PermissionError(
            "The filesystem policy does not permit Windows reserved device names"
        )
    if os.name == "nt" and _contains_windows_alternate_data_stream(value):
        raise PermissionError(
            "The filesystem policy does not permit Windows alternate data streams"
        )


def _contains_windows_reserved_device_name(value: str) -> bool:
    for component in value.replace("/", "\\").split("\\"):
        component = component.rstrip(" .")
        if not component or re.fullmatch(r"[A-Za-z]:", component):
            continue
        basename = component.split(":", 1)[0].split(".", 1)[0].rstrip(" .")
        if _WINDOWS_RESERVED_DEVICE_RE.fullmatch(basename):
            return True
    return False


def _contains_windows_alternate_data_stream(value: str) -> bool:
    windows_path = PureWindowsPath(value)
    normalized = value.replace("/", "\\")
    drive = windows_path.drive
    if drive and normalized.casefold().startswith(drive.casefold()):
        normalized = normalized[len(drive) :]
    return ":" in normalized


def _reject_remote_windows_drive(path: Path) -> None:
    """Reject mapped network drives while allowing fixed and removable volumes."""

    if os.name != "nt" or not path.anchor:
        return
    if _windows_drive_type(path.anchor) == 4:  # DRIVE_REMOTE
        raise PermissionError(
            "The filesystem policy does not permit mapped network drives"
        )


def _windows_drive_type(root: str) -> int:
    """Return the Win32 drive type for an absolute drive root."""

    import ctypes

    get_drive_type = ctypes.windll.kernel32.GetDriveTypeW
    get_drive_type.argtypes = [ctypes.c_wchar_p]
    get_drive_type.restype = ctypes.c_uint
    return int(get_drive_type(root))


def _reject_multiply_linked_regular_file(path: Path) -> None:
    try:
        metadata = path.lstat()
    except FileNotFoundError:
        return
    except OSError as err:
        raise PermissionError(
            "The filesystem policy could not validate hard-link metadata"
        ) from err
    if stat.S_ISREG(metadata.st_mode) and metadata.st_nlink > 1:
        raise PermissionError(
            "The filesystem policy does not permit writes through files with "
            "multiple hard links"
        )


def _validated_network_origin(value: str) -> str:
    parsed = urlsplit(value.strip())
    if (
        parsed.scheme.lower() not in {"http", "https"}
        or parsed.hostname != "127.0.0.1"
        or parsed.username is not None
        or parsed.password is not None
        or parsed.path not in {"", "/"}
        or parsed.query
        or parsed.fragment
    ):
        raise ValueError(
            "security_policy.network.allowed_origins entries must be exact "
            "http(s)://127.0.0.1:<port> origins"
        )
    try:
        port = parsed.port
    except ValueError as err:
        raise ValueError(
            "security_policy.network.allowed_origins contains an invalid port"
        ) from err
    if port is None:
        raise ValueError(
            "security_policy.network.allowed_origins entries require an explicit port"
        )
    return f"{parsed.scheme.lower()}://127.0.0.1:{port}"


def _request_network_origin(value: str) -> str:
    parsed = urlsplit(value.strip())
    if (
        parsed.scheme.lower() not in {"http", "https"}
        or parsed.hostname != "127.0.0.1"
        or parsed.username is not None
        or parsed.password is not None
        or parsed.fragment
    ):
        raise PermissionError(
            "Formal restricted mode permits only approved loopback HTTP sources"
        )
    try:
        port = parsed.port
    except ValueError as err:
        raise PermissionError("Network source contains an invalid port") from err
    if port is None:
        raise PermissionError("Network source must use an explicit approved port")
    return f"{parsed.scheme.lower()}://127.0.0.1:{port}"


def _reject_reparse_components(path: Path, *, stop_at: Path | None = None) -> None:
    current = path
    while True:
        is_junction = getattr(current, "is_junction", None)
        if current.is_symlink() or bool(is_junction and is_junction()):
            raise PermissionError(
                f"The filesystem policy rejects symbolic links and junctions: {current}"
            )
        if stop_at is not None and current == stop_at:
            break
        parent = current.parent
        if parent == current:
            break
        current = parent
