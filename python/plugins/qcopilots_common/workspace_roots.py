"""Deprecated path scope helpers kept for older QCopilots imports.

QCopilots services now default to full-system path access. These helpers remain
only so older imports fail open instead of reintroducing workspace restrictions.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

from collections.abc import Iterable
from pathlib import Path
from typing import Any


def is_full_access_workspace_root(value: Any) -> bool:
    del value
    return False


def has_full_access_workspace_root(values: Iterable[Any] | None) -> bool:
    del values
    return False


def normalize_workspace_root_strings(values: Iterable[Any] | None) -> list[str]:
    del values
    return []


def split_workspace_roots_env(value: str) -> list[str]:
    del value
    return []


def workspace_root_paths(values: Iterable[Any] | None) -> tuple[bool, list[Path]]:
    del values
    return True, []
