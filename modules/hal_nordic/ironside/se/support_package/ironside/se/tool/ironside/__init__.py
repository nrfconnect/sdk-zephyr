# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

"""
Common utilities.
"""

from __future__ import annotations

import argparse
import logging
from collections.abc import Callable
from dataclasses import dataclass


def _init_logger(name: str) -> logging.Logger:
    formatter = logging.Formatter("{message}", style="{")
    handler = logging.StreamHandler()
    handler.setFormatter(formatter)
    logger = logging.getLogger(name)
    logger.setLevel(logging.ERROR)
    logger.addHandler(handler)

    return logger


log = _init_logger("ironside")
"""Shared logging.Logger used by the package"""


class CommandError(RuntimeError):
    """Generic error raised by subcommands"""

    ...


SubParsers = argparse._SubParsersAction
"""The type returned by ArgumentParser.add_subparsers()"""


@dataclass
class SubCommand:
    add_parser: Callable[[SubParsers], argparse.ArgumentParser]
    """Add subcommand argument parser for the subcommand."""

    cmd_handler: Callable[..., None]
    """Execute the subcommand.

    If cmd_handler_unpack_args is True, the function is called like:
        cmd_handler(**vars(args))
    otherwise it is called like:
        cmd_handler(args)
    """

    cmd_handler_unpack_args: bool = True
    """Whether to pass command line arguments as kwargs."""
