"""Thread-safe cooperative interruption of an SSH connection and its resources."""

from __future__ import annotations

import socket
import threading

from .file_transfer_errors import FileTransferError


class ConnectionCancelled(FileTransferError):
    def __init__(self):
        super().__init__("connection_cancelled", "The SSH connection was cancelled.")


def _close_resource(resource, kind):
    if kind == "socket":
        try:
            resource.shutdown(socket.SHUT_RDWR)
        except Exception:
            pass
    try:
        resource.close()
    except Exception:
        pass


class CancellationController:
    """Close registered resources without holding the registration lock.

    Cancellation is one-way. The socket remains registered while Transport and
    SFTP are active so cancellation can unblock their I/O before closing them.
    DNS resolution itself cannot be interrupted by this controller.
    """

    def __init__(self):
        self._lock = threading.Lock()
        self._cancelled = threading.Event()
        self._resources = {}

    @property
    def cancelled(self):
        return self._cancelled.is_set()

    def check(self):
        if self.cancelled:
            raise ConnectionCancelled()

    def register(self, resource, *, kind="resource"):
        with self._lock:
            cancelled = self.cancelled
            if not cancelled:
                self._resources[id(resource)] = (resource, kind)
        if cancelled:
            _close_resource(resource, kind)
            raise ConnectionCancelled()
        return resource

    def unregister(self, resource):
        with self._lock:
            return self._resources.pop(id(resource), None) is not None

    def cancel(self):
        with self._lock:
            if self.cancelled:
                return
            self._cancelled.set()
            resources = tuple(self._resources.values())
            self._resources.clear()
        priority = {"socket": 0, "transport": 1, "sftp": 2}
        for resource, kind in sorted(resources, key=lambda item: priority.get(item[1], 3)):
            _close_resource(resource, kind)
