"""MCP descriptors for configuration and asynchronous SSH file transfers."""

from __future__ import annotations

import json
import threading

from qcopilots_common.mcp_http import McpTool

from .file_transfer_errors import FileTransferError, public_error, public_value


_PATH = {"type": "string", "minLength": 1, "description": "Absolute local path or server path, according to the transfer direction."}
_SOURCES = {"type": "array", "items": _PATH, "minItems": 1, "description": "Files and directories to copy, preserving each source basename."}
_JOB_ID = {"type": "string", "minLength": 1, "description": "Job ID returned by a start, list or resume operation."}
_REQUEST_ID = {"type": "string", "minLength": 1, "description": "Optional idempotency key for this request."}
_CONFIG_PROPERTIES = {
    "host": {"type": "string", "description": "SSH server hostname or IP address, without an account or URL prefix."},
    "port": {"type": "integer", "minimum": 1, "maximum": 65535, "description": "SSH server port, usually 22."},
    "username": {"type": "string", "description": "SSH account name."},
    "password": {"type": "string", "description": "Password saved as plaintext in the local configuration. Omit to keep the existing password. An empty string clears it. Never repeat this value in the response."},
    "upload_sources": {"type": "array", "items": _PATH, "description": "Default local upload files or directories. An empty list clears the defaults."},
    "upload_destination": {"type": "string", "description": "Default remote upload destination directory, for example C:/Data/QGISData/."},
    "download_sources": {"type": "array", "items": _PATH, "description": "Default remote download files or directories. An empty list clears the defaults."},
    "download_destination": {"type": "string", "description": "Default absolute local download destination directory."},
    "connect_timeout": {"type": "number", "exclusiveMinimum": 0, "maximum": 3600, "description": "Connection and authentication timeout in seconds."},
    "io_timeout": {"type": "number", "exclusiveMinimum": 0, "maximum": 3600, "description": "Socket I/O timeout in seconds."},
    "known_hosts_file": {"type": "string", "description": "Read-only OpenSSH known_hosts file. Omitted or empty values use ~/.ssh/known_hosts."},
    "max_bytes_per_second": {"type": "integer", "minimum": 0, "maximum": 9223372036854775807, "description": "Optional bandwidth limit in bytes per second. Zero means unlimited."},
}


def _schema(properties=None, required=()):
    return {
        "type": "object",
        "properties": properties or {},
        "required": list(required),
        "additionalProperties": False,
    }


class FileTransferManagerProvider:
    """Create transfer state on demand and shut it down with its owning server."""

    def __init__(self, manager=None, factory=None):
        self._manager = manager
        self._factory = factory
        self._lock = threading.Lock()
        self._closed = False

    def get(self):
        with self._lock:
            if self._closed:
                raise FileTransferError("service_stopping", "The transfer service is stopping.", retryable=True)
            if self._manager is None:
                if self._factory is not None:
                    self._manager = self._factory()
                    return self._manager
                from .file_transfer_jobs import FileTransferJobManager

                self._manager = FileTransferJobManager()
            return self._manager

    def shutdown(self):
        with self._lock:
            if self._closed:
                return
            self._closed = True
            manager = self._manager
        if manager is not None:
            manager.shutdown()


def build_file_transfer_tools(manager=None, manager_provider=None):
    """Build descriptors without importing Paramiko or opening the job store."""

    if manager is not None and manager_provider is not None:
        raise ValueError("Provide a manager or its provider, not both")
    provider = manager_provider if manager_provider is not None else FileTransferManagerProvider(manager)

    def tool(name, description, schema, callback):
        def handler(arguments):
            secrets = []
            if isinstance(arguments, dict) and isinstance(arguments.get("password"), str):
                secrets.append(arguments["password"])
            try:
                current = provider.get()
                try:
                    stored = current.config_store.load()
                    secrets.append(stored.get("password", ""))
                except Exception:
                    # The actual operation reports an invalid config with a safe error.
                    pass
                result = callback(current, arguments)
                return public_value(result, secrets)
            except Exception as error:
                result = {"error": public_error(error, secrets)}
                return {
                    "content": [{"type": "text", "text": json.dumps(result, ensure_ascii=False)}],
                    "structuredContent": result,
                    "isError": True,
                }

        return McpTool(name, description, schema, handler)

    transfer_properties = {
        "sources": _SOURCES,
        "destination": _PATH,
        "overwrite": {"type": "boolean", "default": False, "description": "Explicitly allow atomic replacement of existing files. Unsupported atomic replacement is rejected."},
        "client_request_id": _REQUEST_ID,
    }

    def start(direction):
        return lambda current, args: current.start(
            direction,
            sources=args.get("sources"),
            destination=args.get("destination"),
            overwrite=args.get("overwrite", False),
            client_request_id=args.get("client_request_id"),
        )

    return [
        tool(
            "file_transfer_get_config",
            "Read the active file upload and download settings and configuration file path. Transfers use SSH/SFTP. Passwords are never returned.",
            _schema(),
            lambda current, args: current.config_store.public(),
        ),
        tool(
            "file_transfer_update_config",
            "Save explicit changes to the file upload and download settings, including the SSH/SFTP server and default paths. Passwords are stored as plaintext by design but never returned. Omitted fields keep their current values. Changes affect new transfers.",
            _schema(_CONFIG_PROPERTIES),
            lambda current, args: current.config_store.update(args),
        ),
        tool(
            "file_transfer_test_connection",
            "Test the configured file transfer server connection over SSH/SFTP. An unknown host returns its SHA256 fingerprint and a confirmation token without trusting it. Ask the user to confirm that exact fingerprint before calling file_transfer_trust_host_key.",
            _schema(),
            lambda current, args: current.backend.test_connection(current.config_store.load(require_connection=True)),
        ),
        tool(
            "file_transfer_trust_host_key",
            "Trust the exact new SSH host fingerprint after the user explicitly confirms it. Use the fingerprint and one-time confirmation token from file_transfer_test_connection. Never infer approval or automatically trust a changed known host key.",
            _schema(
                {
                    "fingerprint": {"type": "string", "minLength": 1, "description": "Exact SHA256 fingerprint explicitly confirmed by the user."},
                    "confirmation_token": {"type": "string", "minLength": 1, "description": "One-time confirmation token returned for this exact host and fingerprint."},
                },
                ("fingerprint", "confirmation_token"),
            ),
            lambda current, args: current.backend.trust_host(
                current.config_store.load(require_connection=True),
                args["fingerprint"],
                args["confirmation_token"],
            ),
        ),
        tool(
            "file_transfer_start_upload",
            "Upload local files, directories or a mixed list to the server in an asynchronous SFTP job. Omitted paths use configured defaults. Each source basename is retained under the destination directory. Missing destination directories are created recursively. Poll file_transfer_get_job until terminal. Completed files receive full SHA256 verification.",
            _schema(transfer_properties),
            start("upload"),
        ),
        tool(
            "file_transfer_start_download",
            "Download remote files, directories or a mixed list to this computer in an asynchronous SFTP job. Omitted paths use configured defaults. Each source basename is retained under the local destination directory. Missing destination directories are created recursively. Poll file_transfer_get_job until terminal. Completed files receive full SHA256 verification.",
            _schema(transfer_properties),
            start("download"),
        ),
        tool(
            "file_transfer_get_job",
            "Get transfer progress, SHA256 results, resumability and cleanup_pending for one job. File results are paginated. Follow next_files_offset until null to inspect all files. A started job is not a completed transfer.",
            _schema({
                "job_id": _JOB_ID,
                "files_offset": {"type": "integer", "minimum": 0, "default": 0, "description": "Starting offset in the file result list."},
                "files_limit": {"type": "integer", "minimum": 1, "maximum": 200, "default": 100, "description": "Maximum file results returned in this page."},
            }, ("job_id",)),
            lambda current, args: current.get_job(args["job_id"], files_offset=args.get("files_offset", 0), files_limit=args.get("files_limit", 100)),
        ),
        tool(
            "file_transfer_list_jobs",
            "List file upload and download job summaries, including cancelled or interrupted transfers available for resume and pending cleanup. Use file_transfer_get_job to read paginated file details.",
            _schema(
                {
                    "states": {"type": "array", "items": {"type": "string", "enum": ["queued", "running", "cancelling", "succeeded", "failed", "cancelled"]}, "description": "Optional job state filter."},
                    "limit": {"type": "integer", "minimum": 1, "maximum": 200, "default": 100, "description": "Maximum number of jobs returned, from 1 to 200."},
                }
            ),
            lambda current, args: current.list_jobs(states=args.get("states"), limit=args.get("limit", 100)),
        ),
        tool(
            "file_transfer_cancel_job",
            "Stop an active transfer while retaining its partial files and resume checkpoint. Poll until cancelled with resumable=true before resuming. To abandon retained data, use file_transfer_discard after the job stops. Published complete files remain intact.",
            _schema({"job_id": _JOB_ID}, ("job_id",)),
            lambda current, args: current.cancel_job(args["job_id"]),
        ),
        tool(
            "file_transfer_resume_job",
            "Resume a cancelled or interrupted transfer after validating its checkpoint, source and partial contents. Uses current credentials but requires the same connection and source/destination identity. Returns a new job_id for the same transfer_id. Discarded transfers cannot resume.",
            _schema({"job_id": _JOB_ID, "client_request_id": _REQUEST_ID}, ("job_id",)),
            lambda current, args: current.resume_job(args["job_id"], client_request_id=args.get("client_request_id")),
        ),
        tool(
            "file_transfer_discard",
            "Abandon a stopped transfer and delete its checkpoint and owned partial files, or retry cleanup_pending. Cancel an active job and wait for it to stop first. Discarded transfers cannot resume. Published complete files are retained.",
            _schema({"transfer_id": {"type": "string", "minLength": 1, "description": "Persistent transfer ID returned in a job snapshot."}}, ("transfer_id",)),
            lambda current, args: current.discard_transfer(args["transfer_id"]),
        ),
    ]
