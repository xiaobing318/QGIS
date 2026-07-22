"""Service identifier validation shared by QCopilots services.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import os
import re


SERVICE_ID_PATTERN = re.compile(r"^[A-Za-z0-9_.-]+$")


def is_safe_service_id(service_id: str) -> bool:
    if not service_id or not SERVICE_ID_PATTERN.fullmatch(service_id):
        return False
    segments = service_id.split(".")
    if service_id in (".", "..") or any(not segment or segment == ".." for segment in segments):
        return False
    separators = {"/", "\\", ":", os.sep}
    if os.altsep:
        separators.add(os.altsep)
    return not any(separator in service_id for separator in separators if separator)


def safe_service_state_name(service_id: str) -> str:
    if not is_safe_service_id(service_id):
        raise ValueError(f"Invalid QCopilots service id: {service_id}")
    return service_id.replace(".", "_")
