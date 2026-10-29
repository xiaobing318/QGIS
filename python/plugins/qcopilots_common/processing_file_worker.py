"""Bounded, cancellable file work for Processing jobs.

Only plain file data may cross this boundary. QGIS objects are owned by the
manager on the application thread, including the completion callback.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from contextlib import contextmanager
import threading


class ProcessingFileCancelled(RuntimeError):
    """File work was cancelled before its next safe boundary."""


_local = threading.local()
_slots = threading.BoundedSemaphore(2)


@contextmanager
def file_work_context(cancel_event=None, progress=None):
    previous = getattr(_local, "work", None)
    _local.work = {"cancel": cancel_event, "progress": progress}
    try:
        check_file_cancelled()
        yield
    finally:
        _local.work = previous


def in_file_work():
    return getattr(_local, "work", None) is not None


def check_file_cancelled():
    work = getattr(_local, "work", None)
    if work and work["cancel"] is not None and work["cancel"].is_set():
        raise ProcessingFileCancelled("Processing file operation cancelled")


def report_file_progress(completed, total):
    work = getattr(_local, "work", None)
    if work and work["progress"] is not None:
        work["progress"](int(completed), int(total))


def make_file_task(function, completed, *, cancel_event=None, progress=None):
    """Create a QGIS task whose finished callback runs on the main thread."""
    from qgis.core import QgsTask

    class FileTask(QgsTask):
        def __init__(self):
            super().__init__("QCopilots Processing file operation", QgsTask.Flag.CanCancel)
            self.value = None
            self.error = None

        def run(self):
            acquired = False
            try:
                with file_work_context(cancel_event, progress):
                    while not acquired:
                        check_file_cancelled()
                        acquired = _slots.acquire(timeout=0.1)
                    self.value = function()
                return True
            except Exception as error:
                self.error = error
                return False
            finally:
                if acquired:
                    _slots.release()

        def cancel(self):
            if cancel_event is not None:
                cancel_event.set()
            super().cancel()

        def finished(self, successful):
            error = self.error
            if not successful and error is None:
                error = ProcessingFileCancelled("Processing file task was not run")
            completed(error, self.value)

    return FileTask()
