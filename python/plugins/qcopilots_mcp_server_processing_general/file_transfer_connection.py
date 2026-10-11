"""Shared validation for saved and directly supplied SSH connection settings."""

from __future__ import annotations

import math

from .file_transfer_errors import FileTransferError


def validate_connection_fields(config, *, required=False, code="invalid_config", missing_code=None):
    """Validate connection fields and return their canonical endpoint.

    Empty fields are valid while editing settings. Neither this validator nor
    its errors include supplied values, which may contain credentials.
    """
    host = config.get("host", "")
    username = config.get("username", "")
    password = config.get("password", "")
    for name, value in (("host", host), ("username", username), ("password", password)):
        if not isinstance(value, str) or "\0" in value:
            raise FileTransferError(code, f"The SSH {name} must be a string without NUL characters.")
    if any(ord(c) < 33 or ord(c) == 127 or c.isspace() or c in ",#[]*/\\?@" for c in host):
        raise FileTransferError(code, "The SSH host must be a hostname or IP address without a URL, account or host pattern.")
    if any(ord(c) < 32 or ord(c) == 127 for c in username):
        raise FileTransferError(code, "The SSH username must not contain control characters.")
    username = username.strip()
    port = config.get("port", 22)
    if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
        raise FileTransferError(code, "The SSH port must be an integer between 1 and 65535.")
    for name, default in (("connect_timeout", 15), ("io_timeout", 30)):
        value = config.get(name, default)
        if (isinstance(value, bool) or not isinstance(value, (int, float))
                or not 0 < value <= 3600 or not math.isfinite(value)):
            raise FileTransferError(code, f"The SSH {name} must be greater than zero and at most 3600 seconds.")
    if required and (not host or not username or not password):
        raise FileTransferError(missing_code or code, "Set the SSH host, username and password before connecting.")
    return host, port, username
