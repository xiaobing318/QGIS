"""Logging helpers that work inside and outside QGIS.

.. note:: This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
"""

from __future__ import annotations

import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path

from qcopilots_common.constants import LOG_CHANNEL, logs_root

LOG_ROTATE_BYTES = 1024 * 1024
LOG_BACKUP_COUNT = 1


def ensure_log_dir() -> Path:
    path = logs_root()
    path.mkdir(parents=True, exist_ok=True)
    return path


def service_log_file(service_id: str) -> Path:
    return ensure_log_dir() / f"{service_id.replace('.', '_')}.log"


def configure_logger(name: str, log_file: str | Path | None = None) -> logging.Logger:
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    logger.propagate = False

    if not logger.handlers:
        stream_handler = logging.StreamHandler()
        stream_handler.setFormatter(
            logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s")
        )
        logger.addHandler(stream_handler)

    if log_file:
        log_path = Path(log_file)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        target_log_file = str(log_path.resolve())
        for handler in list(logger.handlers):
            if isinstance(handler, RotatingFileHandler) and _handler_log_file(handler) != target_log_file:
                logger.removeHandler(handler)
                handler.close()
        existing_files = {
            _handler_log_file(handler) for handler in logger.handlers
        }
        if target_log_file not in existing_files:
            file_handler = RotatingFileHandler(
                log_path,
                maxBytes=LOG_ROTATE_BYTES,
                backupCount=LOG_BACKUP_COUNT,
                encoding="utf-8",
            )
            file_handler.setFormatter(
                logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s")
            )
            logger.addHandler(file_handler)

    return logger


def _handler_log_file(handler: logging.Handler) -> str | None:
    base_filename = getattr(handler, "baseFilename", None)
    if not base_filename:
        return None
    return str(Path(base_filename).resolve())


def qgis_log(message: str, level: str = "info") -> None:
    """Log to the QGIS message log when QGIS is available."""

    try:
        from qgis.core import Qgis, QgsMessageLog
    except Exception:
        logging.getLogger(LOG_CHANNEL).log(_python_level(level), message)
        return

    qgis_levels = {
        "debug": Qgis.MessageLevel.Info,
        "info": Qgis.MessageLevel.Info,
        "warning": Qgis.MessageLevel.Warning,
        "error": Qgis.MessageLevel.Critical,
        "critical": Qgis.MessageLevel.Critical,
    }
    QgsMessageLog.logMessage(message, LOG_CHANNEL, qgis_levels.get(level, Qgis.MessageLevel.Info))


def _python_level(level: str) -> int:
    return {
        "debug": logging.DEBUG,
        "info": logging.INFO,
        "warning": logging.WARNING,
        "error": logging.ERROR,
        "critical": logging.CRITICAL,
    }.get(level, logging.INFO)


def rotate_log_file(path: str | Path, max_bytes: int = LOG_ROTATE_BYTES) -> None:
    log_path = Path(path)
    if not log_path.exists() or log_path.stat().st_size < max_bytes:
        return
    backup = log_path.with_name(f"{log_path.name}.1")
    if backup.exists():
        backup.unlink()
    log_path.replace(backup)
