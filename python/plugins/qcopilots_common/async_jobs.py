"""Thread-safe in-memory storage shared by QCopilots asynchronous jobs.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import copy
import math
import threading
import time
import uuid
from collections.abc import Callable, Iterable
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any


PUBLIC_JOB_STATES = frozenset(
    {"queued", "running", "cancelling", "succeeded", "failed", "cancelled"}
)
TERMINAL_JOB_STATES = frozenset({"succeeded", "failed", "cancelled"})

_ALLOWED_TRANSITIONS = {
    "queued": frozenset({"running", "cancelling", "failed", "cancelled"}),
    "running": frozenset({"cancelling", "succeeded", "failed", "cancelled"}),
    "cancelling": frozenset({"succeeded", "failed", "cancelled"}),
    "succeeded": frozenset(),
    "failed": frozenset(),
    "cancelled": frozenset(),
}


class IdempotencyConflictError(RuntimeError):
    """Raised when an idempotency key is reused for a different request."""


class JobNotFoundError(RuntimeError):
    """Raised when a requested job does not exist in the requested scope."""


class InvalidJobTransitionError(RuntimeError):
    """Raised when a caller attempts an invalid public state transition."""


def utc_now_rfc3339() -> str:
    """Return a UTC RFC 3339 timestamp suitable for a public job snapshot."""

    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


@dataclass
class _JobEntry:
    scope: str
    snapshot: dict[str, Any]
    request_fingerprint: str | None
    created_sequence: int
    runtime: Any = None
    terminal_monotonic: float | None = None
    terminal_sequence: int | None = None


class AsyncJobStore:
    """Owns public snapshots and private runtime state for asynchronous jobs.

    Public data is always copied while the runtime value is deliberately opaque.
    This prevents HTTP response serialization or test code from accidentally
    copying QGIS tasks, contexts, feedback objects, or Qt objects.
    """

    def __init__(
        self,
        *,
        scope_field: str = "category",
        terminal_ttl_seconds: float = 24 * 60 * 60,
        max_terminal_jobs_per_scope: int = 200,
        now: Callable[[], float] | None = None,
        on_evict: Callable[[str, Any], None] | None = None,
    ):
        ttl = float(terminal_ttl_seconds)
        if not math.isfinite(ttl) or ttl < 0:
            raise ValueError("terminal_ttl_seconds must be finite and non-negative")
        if isinstance(max_terminal_jobs_per_scope, bool) or int(
            max_terminal_jobs_per_scope
        ) < 0:
            raise ValueError("max_terminal_jobs_per_scope must be non-negative")
        self.scope_field = str(scope_field)
        self.terminal_ttl_seconds = ttl
        self.max_terminal_jobs_per_scope = int(max_terminal_jobs_per_scope)
        self._now = now or time.monotonic
        self._on_evict = on_evict
        self._lock = threading.RLock()
        self._jobs: dict[str, _JobEntry] = {}
        self._idempotency: dict[tuple[str, str], str] = {}
        self._accepting = True
        self._next_created_sequence = 0
        self._next_terminal_sequence = 0

    @property
    def accepting(self) -> bool:
        with self._lock:
            return self._accepting

    def lookup_idempotent(
        self,
        scope: str,
        client_request_id: str | None,
        request_fingerprint: str | None,
    ) -> dict[str, Any] | None:
        """Return the existing snapshot for a matching idempotent request."""

        if not client_request_id:
            return None
        with self._lock:
            self._purge_locked()
            job_id = self._idempotency.get((str(scope), client_request_id))
            if job_id is None:
                return None
            entry = self._jobs.get(job_id)
            if entry is None:
                self._idempotency.pop((str(scope), client_request_id), None)
                return None
            if entry.request_fingerprint != request_fingerprint:
                raise IdempotencyConflictError(
                    "idempotency_conflict: client_request_id was already used "
                    "for a different request"
                )
            return copy.deepcopy(entry.snapshot)

    def create(
        self,
        scope: str,
        snapshot: dict[str, Any],
        *,
        client_request_id: str | None = None,
        request_fingerprint: str | None = None,
        runtime: Any = None,
    ) -> tuple[dict[str, Any], bool]:
        """Atomically create a job or return its idempotent predecessor."""

        normalized_scope = str(scope)
        with self._lock:
            if not self._accepting:
                raise RuntimeError("QCopilots job manager is stopping")
            self._purge_locked()
            existing = self.lookup_idempotent(
                normalized_scope, client_request_id, request_fingerprint
            )
            if existing is not None:
                return existing, False

            public_snapshot = copy.deepcopy(snapshot)
            job_id = str(public_snapshot.get("job_id") or uuid.uuid4())
            try:
                parsed_job_id = uuid.UUID(job_id)
            except (ValueError, TypeError, AttributeError) as err:
                raise ValueError("job_id must be a UUID") from err
            if parsed_job_id.version != 4:
                raise ValueError("job_id must be a UUIDv4")
            if job_id in self._jobs:
                raise ValueError(f"Duplicate job_id: {job_id}")

            state = str(public_snapshot.get("state", "queued"))
            if state not in PUBLIC_JOB_STATES:
                raise ValueError(f"Invalid public job state: {state}")
            public_snapshot["job_id"] = job_id
            public_snapshot[self.scope_field] = normalized_scope
            public_snapshot["state"] = state
            self._next_created_sequence += 1
            terminal_sequence = None
            if state in TERMINAL_JOB_STATES:
                self._next_terminal_sequence += 1
                terminal_sequence = self._next_terminal_sequence
            entry = _JobEntry(
                scope=normalized_scope,
                snapshot=public_snapshot,
                request_fingerprint=request_fingerprint,
                created_sequence=self._next_created_sequence,
                runtime=runtime,
                terminal_monotonic=self._now()
                if state in TERMINAL_JOB_STATES
                else None,
                terminal_sequence=terminal_sequence,
            )
            self._jobs[job_id] = entry
            if client_request_id:
                self._idempotency[(normalized_scope, client_request_id)] = job_id
            self._enforce_terminal_limit_locked(normalized_scope)
            return copy.deepcopy(public_snapshot), True

    def get(self, job_id: str, scope: str | None = None) -> dict[str, Any]:
        with self._lock:
            self._purge_locked()
            entry = self._entry_locked(job_id, scope)
            return copy.deepcopy(entry.snapshot)

    def list(
        self,
        scope: str,
        *,
        states: Iterable[str] | None = None,
        limit: int = 100,
    ) -> list[dict[str, Any]]:
        if isinstance(limit, bool) or int(limit) < 1 or int(limit) > 200:
            raise ValueError("limit must be an integer from 1 through 200")
        requested_states = None
        if states is not None:
            requested_states = {str(state) for state in states}
            invalid_states = requested_states - PUBLIC_JOB_STATES
            if invalid_states:
                raise ValueError(
                    "Invalid public job states: " + ", ".join(sorted(invalid_states))
                )
        with self._lock:
            self._purge_locked()
            matches = [
                (entry.created_sequence, copy.deepcopy(entry.snapshot))
                for entry in self._jobs.values()
                if entry.scope == str(scope)
                and (
                    requested_states is None
                    or entry.snapshot["state"] in requested_states
                )
            ]
        matches.sort(key=lambda item: item[0], reverse=True)
        return [snapshot for _, snapshot in matches[: int(limit)]]

    def patch(self, job_id: str, scope: str | None = None, **changes: Any) -> dict[str, Any]:
        """Patch non-state fields without allowing a terminal job to change."""

        if "state" in changes:
            raise ValueError("Use transition() to change a job state")
        with self._lock:
            entry = self._entry_locked(job_id, scope)
            if entry.snapshot["state"] in TERMINAL_JOB_STATES:
                return copy.deepcopy(entry.snapshot)
            entry.snapshot.update(copy.deepcopy(changes))
            return copy.deepcopy(entry.snapshot)

    def transition(
        self,
        job_id: str,
        state: str,
        scope: str | None = None,
        **changes: Any,
    ) -> dict[str, Any]:
        """Move a job through the public state machine exactly once at terminal."""

        target_state = str(state)
        if target_state not in PUBLIC_JOB_STATES:
            raise ValueError(f"Invalid public job state: {target_state}")
        with self._lock:
            entry = self._entry_locked(job_id, scope)
            current_state = entry.snapshot["state"]
            if current_state in TERMINAL_JOB_STATES:
                return copy.deepcopy(entry.snapshot)
            if target_state != current_state and target_state not in _ALLOWED_TRANSITIONS[
                current_state
            ]:
                raise InvalidJobTransitionError(
                    f"Invalid job transition: {current_state} -> {target_state}"
                )
            entry.snapshot.update(copy.deepcopy(changes))
            entry.snapshot["state"] = target_state
            if target_state in TERMINAL_JOB_STATES:
                entry.terminal_monotonic = self._now()
                self._next_terminal_sequence += 1
                entry.terminal_sequence = self._next_terminal_sequence
                self._enforce_terminal_limit_locked(entry.scope)
            return copy.deepcopy(entry.snapshot)

    def request_cancel(
        self, job_id: str, scope: str | None = None
    ) -> tuple[dict[str, Any], Any, bool]:
        """Atomically mark a cancellable active job as cancelling."""

        with self._lock:
            entry = self._entry_locked(job_id, scope)
            state = entry.snapshot["state"]
            if state in TERMINAL_JOB_STATES:
                return copy.deepcopy(entry.snapshot), entry.runtime, False
            if not entry.snapshot.get("cancellable", False):
                raise RuntimeError("job_not_cancellable: this job cannot be cancelled")
            entry.snapshot["cancel_requested"] = True
            if state != "cancelling":
                entry.snapshot["state"] = "cancelling"
            return copy.deepcopy(entry.snapshot), entry.runtime, True

    def set_runtime(self, job_id: str, runtime: Any, scope: str | None = None) -> None:
        with self._lock:
            self._entry_locked(job_id, scope).runtime = runtime

    def get_runtime(self, job_id: str, scope: str | None = None) -> Any:
        with self._lock:
            return self._entry_locked(job_id, scope).runtime

    def active_runtime_items(self) -> list[tuple[str, str, Any]]:
        with self._lock:
            return [
                (job_id, entry.scope, entry.runtime)
                for job_id, entry in self._jobs.items()
                if entry.snapshot["state"] not in TERMINAL_JOB_STATES
            ]

    def purge(self) -> None:
        with self._lock:
            self._purge_locked()

    def shutdown(self) -> None:
        """Reject future creations while retaining snapshots and live runtimes."""

        with self._lock:
            self._accepting = False

    def _entry_locked(self, job_id: str, scope: str | None) -> _JobEntry:
        entry = self._jobs.get(str(job_id))
        if entry is None or (scope is not None and entry.scope != str(scope)):
            raise JobNotFoundError(f"job_not_found: {job_id}")
        return entry

    def _purge_locked(self) -> None:
        cutoff = self._now() - self.terminal_ttl_seconds
        expired = [
            job_id
            for job_id, entry in self._jobs.items()
            if entry.terminal_monotonic is not None
            and entry.terminal_monotonic <= cutoff
        ]
        for job_id in expired:
            self._evict_locked(job_id)

    def _enforce_terminal_limit_locked(self, scope: str) -> None:
        terminal = sorted(
            (
                (
                    entry.terminal_monotonic or 0.0,
                    entry.terminal_sequence or 0,
                    job_id,
                )
                for job_id, entry in self._jobs.items()
                if entry.scope == scope
                and entry.snapshot["state"] in TERMINAL_JOB_STATES
            )
        )
        excess = len(terminal) - self.max_terminal_jobs_per_scope
        for _, _, job_id in terminal[: max(0, excess)]:
            self._evict_locked(job_id)

    def _evict_locked(self, job_id: str) -> None:
        entry = self._jobs.pop(job_id, None)
        if entry is None:
            return
        client_request_id = entry.snapshot.get("client_request_id")
        if client_request_id:
            self._idempotency.pop((entry.scope, client_request_id), None)
        if self._on_evict:
            try:
                self._on_evict(job_id, entry.runtime)
            except Exception:
                # Retention cleanup must never make a public job API fail.
                pass
