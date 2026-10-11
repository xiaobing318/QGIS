"""Controlled errors and public value redaction for SSH file transfers."""

from __future__ import annotations

from collections.abc import Mapping
from typing import Any


class FileTransferError(Exception):
    """A transfer failure whose code is stable across transport implementations."""

    def __init__(self, code, message, retryable=False, details=None):
        super().__init__(message)
        self.code = str(code)
        self.message = str(message)
        self.retryable = bool(retryable)
        self.details = dict(details or {})


_PROTOCOL_FIELDS = frozenset({
    "job_id", "transfer_id", "client_request_id", "request_id", "resumes_job_id",
    "state", "status", "phase", "code", "category", "direction", "protocol",
    "fingerprint", "host_fingerprint", "confirmation_token", "algorithm",
    "sha256", "source_sha256", "committed_prefix_sha256", "request_fingerprint",
    "host", "username", "source", "sources", "target", "stage", "destination",
    "path", "paths", "current_path", "residual_paths", "config_path", "known_hosts_file",
    "upload_sources", "upload_destination", "download_sources", "download_destination",
    "created_at", "updated_at", "finished_at", "kind",
})


def public_value(value: Any, secrets=(), *, _field=None) -> Any:
    """Remove credential fields and redact prose without corrupting protocol data.

    A short password can coincide with an ID, hash, state or path substring.
    Those independently sourced protocol values must retain their exact spelling
    so callers can confirm hosts, query jobs and interpret their results.
    """

    secret_values = tuple(item for item in secrets if isinstance(item, str) and item)
    if isinstance(value, Mapping):
        return {
            str(key): public_value(item, secret_values, _field=str(key).casefold())
            for key, item in value.items()
            if str(key).casefold() not in {"password", "passwd", "passphrase"}
        }
    if isinstance(value, (list, tuple)):
        return [public_value(item, secret_values, _field=_field) for item in value]
    if isinstance(value, str):
        if _field not in _PROTOCOL_FIELDS:
            for secret in secret_values:
                # Async errors pass through both jobs and MCP output filtering.
                # Do not redact characters inside an existing placeholder again.
                value = "[redacted]".join(
                    part.replace(secret, "[redacted]") for part in value.split("[redacted]")
                )
        return value
    if value is None or isinstance(value, (bool, int, float)):
        return value
    # Never stringify arbitrary runtime objects, which may contain credentials.
    return None


def public_error(error: Exception, secrets=()) -> dict[str, Any]:
    """Describe expected failures without serializing arbitrary exception text."""

    if isinstance(error, FileTransferError):
        return public_value(
            {
                "code": error.code,
                "message": error.message,
                "retryable": error.retryable,
                "details": error.details,
            },
            secrets,
        )
    return {
        "code": "internal_error",
        "message": "The file transfer operation failed unexpectedly.",
        "retryable": False,
        "details": {},
    }
