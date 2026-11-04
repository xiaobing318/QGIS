"""MCP descriptors for configuration and asynchronous SSH file transfers."""

from __future__ import annotations

import json
import threading

from qcopilots_common.mcp_http import McpTool

from .file_transfer_errors import FileTransferError, public_error, public_value


_PATH = {"type": "string", "minLength": 1, "description": "Absolute local path or server path, according to the transfer direction."}
_SOURCES = {"type": "array", "items": _PATH, "minItems": 1, "description": "Absolute files and directories to copy, local for uploads and on the server for downloads. Each source keeps its basename under the destination."}
_DESTINATION = {**_PATH, "description": "One absolute destination directory, on the server for uploads and local for downloads. It is always a directory that receives each source by name, never a target file name."}
_JOB_ID = {"type": "string", "minLength": 1, "description": "Job ID returned by a start, list or resume operation."}
_REQUEST_ID = {"type": "string", "minLength": 1, "description": "Optional idempotency key. Repeating the same request with the same key returns the existing job instead of starting another one."}
_CONFIG_PROPERTIES = {
    "host": {"type": "string", "description": "SSH server hostname or IP address, without an account or URL prefix."},
    "port": {"type": "integer", "minimum": 1, "maximum": 65535, "description": "SSH server port, usually 22."},
    "username": {"type": "string", "description": "SSH account name."},
    "password": {"type": "string", "description": "Password saved as plaintext in the local configuration. Omit to keep the existing password. An empty string clears it. Never repeat this value in the response."},
    "upload_sources": {"type": "array", "items": _PATH, "description": "Default absolute local upload files or directories. An empty list clears the defaults."},
    "upload_destination": {"type": "string", "description": "Default absolute remote upload destination directory, for example C:/Data/QGISData/. An empty string clears the default."},
    "download_sources": {"type": "array", "items": _PATH, "description": "Default absolute remote download files or directories. An empty list clears the defaults."},
    "download_destination": {"type": "string", "description": "Default absolute local download destination directory. An empty string clears the default."},
    "connect_timeout": {"type": "number", "exclusiveMinimum": 0, "maximum": 3600, "description": "Connection and authentication timeout in seconds."},
    "io_timeout": {"type": "number", "exclusiveMinimum": 0, "maximum": 3600, "description": "Socket I/O timeout in seconds."},
    "known_hosts_file": {"type": "string", "description": "Read-only OpenSSH known_hosts file, given as an absolute path or a path starting with ~. Omitted or empty values use ~/.ssh/known_hosts."},
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
        "destination": _DESTINATION,
        "overwrite": {"type": "boolean", "default": False, "description": "Explicitly allow atomic replacement of existing target files, which otherwise make the job fail with destination_conflict. Unsupported atomic replacement is rejected with overwrite_unsupported and the original is kept."},
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
            "Read the active file upload and download settings and configuration file path, without changing anything. Transfers use SSH/SFTP with password login. Returns host, port, username, the default upload and download sources and destinations, timeouts, known_hosts_file, max_bytes_per_second, config_path and password_configured. Passwords are never returned. Start here. The usual order is file_transfer_update_config when host, username or password is missing, file_transfer_test_connection, file_transfer_trust_host_key only for a new host the user confirmed, file_transfer_start_upload or file_transfer_start_download, then poll file_transfer_get_job. For a copy between two local paths use copy_file instead.",
            _schema(),
            lambda current, args: current.config_store.public(),
        ),
        tool(
            "file_transfer_update_config",
            "Save explicit changes to the file upload and download settings, including the SSH/SFTP server and default paths, and return the same password-free view as file_transfer_get_config. Omitted fields keep their current values. Unknown fields and invalid values are rejected, and a failed save keeps the previous settings. Host, username and password must all be set before connections or transfers can start. This is the only tool that accepts the password, and every other tool reads it from the saved file. Passwords are stored as plaintext by design but never returned, so pass one only when the user provides it for this purpose. Changes affect new transfers and resumes, while running jobs keep the settings they started with. After changing host, port or username, run file_transfer_test_connection again.",
            _schema(_CONFIG_PROPERTIES),
            lambda current, args: current.config_store.update(args),
        ),
        tool(
            "file_transfer_test_connection",
            "Test the configured file transfer server connection over SSH/SFTP without transferring files. Requires saved host, username and password. The host key is checked before the password is sent. A trusted host with a working login returns connected=true, host, port, username, fingerprint and protocol. An unknown host returns its SHA256 fingerprint and a confirmation token without trusting it, as an error with code host_key_unknown whose details hold host, port, algorithm, fingerprint, confirmation_token and expires_in. Ask the user to confirm that exact fingerprint before calling file_transfer_trust_host_key. A key that differs from a trusted entry fails with host_key_changed and must not be trusted. Other errors include authentication_failed, connection_timeout, network_disabled when security policy disables networking and file_transfer_restricted in formal_restricted mode. Run it before the first transfer, because jobs fail on an untrusted host.",
            _schema(),
            lambda current, args: current.backend.test_connection(current.config_store.load(require_connection=True)),
        ),
        tool(
            "file_transfer_trust_host_key",
            "Trust the exact new SSH host fingerprint after the user explicitly confirms it. Use the fingerprint and one-time confirmation token from file_transfer_test_connection. The tool probes the server again, requires the same key and saves it only in the plugin's own known hosts file. The user's OpenSSH known_hosts file is never changed. The token works once and expires after 10 minutes, so on host_key_confirmation_invalid run file_transfer_test_connection again for a new one. Never infer approval or automatically trust a changed known host key. Then run file_transfer_test_connection to confirm the login.",
            _schema(
                {
                    "fingerprint": {"type": "string", "minLength": 1, "description": "Exact SHA256 fingerprint explicitly confirmed by the user."},
                    "confirmation_token": {"type": "string", "minLength": 1, "description": "One-time confirmation token returned in the host_key_unknown error details for this exact host and fingerprint."},
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
            "Upload local files, directories or a mixed list to the server in an asynchronous SFTP job. Returns the first snapshot with job_id, transfer_id, state queued and progress. Sources are absolute local paths, and destination is one absolute server directory such as C:/RemoteData/ or /home/user/data. Omitted paths use configured defaults. Each source basename is retained under the destination directory, so uploading C:/Data/vector-output to C:/RemoteData/ creates C:/RemoteData/vector-output with its full contents. Missing destination directories are created recursively. An existing target file fails the job with destination_conflict unless overwrite=true. Links, UNC paths and mapped network drives are rejected. At most two transfers run at once. Run file_transfer_test_connection first. Poll file_transfer_get_job until terminal. Completed files receive full SHA256 verification before they are published under their final names.",
            _schema(transfer_properties),
            start("upload"),
        ),
        tool(
            "file_transfer_start_download",
            "Download remote files, directories or a mixed list to this computer in an asynchronous SFTP job. Returns the first snapshot with job_id, transfer_id, state queued and progress. Sources are absolute server paths such as C:/RemoteData/result.tif or /home/user/data, and destination is one absolute local directory. Omitted paths use configured defaults. Each source basename is retained under the local destination directory, and directories are copied recursively. Missing destination directories are created recursively. An existing local file fails the job with destination_conflict unless overwrite=true. Downloaded data is not loaded into QGIS, so use load_map_layer afterwards when needed. Run file_transfer_test_connection first. Poll file_transfer_get_job until terminal. Completed files receive full SHA256 verification before they are published under their final names.",
            _schema(transfer_properties),
            start("download"),
        ),
        tool(
            "file_transfer_get_job",
            "Get transfer progress, SHA256 results, resumability and cleanup_pending for one job. Returns state, phase, progress (bytes_total, bytes_completed, files_total, files_completed), errors, transfer_id and one page of file results, each with source, target, kind, state, size, sha256 and verified. File results are paginated. Follow next_files_offset until null to inspect all files. A started job is not a completed transfer, so poll after every start, cancel or resume until the state is succeeded, failed or cancelled. On failure read errors[].code, for example host_key_unknown, authentication_failed, destination_conflict or source_changed. A failed or cancelled job with resumable=true can continue through file_transfer_resume_job.",
            _schema({
                "job_id": _JOB_ID,
                "files_offset": {"type": "integer", "minimum": 0, "default": 0, "description": "Starting offset in the file result list. Use next_files_offset from the previous page."},
                "files_limit": {"type": "integer", "minimum": 1, "maximum": 200, "default": 100, "description": "Maximum file results returned in this page."},
            }, ("job_id",)),
            lambda current, args: current.get_job(args["job_id"], files_offset=args.get("files_offset", 0), files_limit=args.get("files_limit", 100)),
        ),
        tool(
            "file_transfer_list_jobs",
            "List file upload and download job summaries, newest first, including cancelled or interrupted transfers available for resume and pending cleanup. Each resume attempt is its own job sharing the transfer_id. Summaries carry state, phase, progress, resumable and cleanup_pending but no per-file results. History is kept on disk across service restarts. Use it to recover a lost job_id or find resumable work, for example with states failed and cancelled. Use file_transfer_get_job to read paginated file details.",
            _schema(
                {
                    "states": {"type": "array", "items": {"type": "string", "enum": ["queued", "running", "cancelling", "succeeded", "failed", "cancelled"]}, "minItems": 1, "description": "Optional job states to include, for example failed and cancelled to find resumable transfers. Omit it to list every state. An empty list is rejected."},
                    "limit": {"type": "integer", "minimum": 1, "maximum": 200, "default": 100, "description": "Maximum number of jobs returned, from 1 to 200."},
                }
            ),
            lambda current, args: current.list_jobs(states=args.get("states"), limit=args.get("limit", 100)),
        ),
        tool(
            "file_transfer_cancel_job",
            "Stop an active transfer while retaining its partial files and resume checkpoint. The job moves to cancelling and then cancelled. Poll until cancelled with resumable=true before resuming. Cancelling a finished job only returns its snapshot, and cancel_too_late=true means every file was already published. To abandon retained data, use file_transfer_discard after the job stops. Published complete files remain intact.",
            _schema({"job_id": _JOB_ID}, ("job_id",)),
            lambda current, args: current.cancel_job(args["job_id"]),
        ),
        tool(
            "file_transfer_resume_job",
            "Resume a cancelled or interrupted transfer after validating its checkpoint, source and partial contents. It also continues a failed job whose snapshot shows resumable=true, for example after a network error. Uses current credentials but requires the same connection and source/destination identity, so a changed host, port or username is rejected with connection_changed and changed source content makes the new job fail with source_changed. Returns a new job_id for the same transfer_id. Poll that new job_id with file_transfer_get_job. Discarded or completed transfers cannot resume.",
            _schema({"job_id": _JOB_ID, "client_request_id": _REQUEST_ID}, ("job_id",)),
            lambda current, args: current.resume_job(args["job_id"], client_request_id=args.get("client_request_id")),
        ),
        tool(
            "file_transfer_discard",
            "Abandon a stopped transfer and delete its checkpoint and owned partial files, or retry cleanup_pending. It takes the transfer_id from a job snapshot, not the job_id. Cancel an active job and wait for it to stop first. Failed and cancelled transfers can be discarded directly. Returns discarded, cleanup (complete, residual_paths, errors) and cleanup_pending. If the server is unreachable, cleanup_pending stays true, so call it again later. Discarded transfers cannot resume. Published complete files are retained.",
            _schema({"transfer_id": {"type": "string", "minLength": 1, "description": "Persistent transfer_id from a job snapshot, shared by every attempt of one transfer. It is not the job_id."}}, ("transfer_id",)),
            lambda current, args: current.discard_transfer(args["transfer_id"]),
        ),
    ]
