# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

"""IronSide utility tool."""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path
from textwrap import dedent

if __package__ is None:
    sys.path.insert(0, str(Path(__file__).parents[1].resolve()))

from ironside import CommandError, SubCommand, log
from ironside.commands import SUBCOMMANDS


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="ironside",
        description=dedent(
            """\
            This tool provides various commands that can be used when developing local
            domain firmware that interacts with Nordic IronSide SE. See the help text of each
            command for details."""
        ),
        allow_abbrev=False,
    )
    parser.add_argument("-v", "--verbose", action="count", default=0, help="output verbose logs")
    subparsers = parser.add_subparsers(title="commands")

    for subcommand in SUBCOMMANDS:
        subcommand_parser = subcommand.add_parser(subparsers)
        subcommand_parser.set_defaults(_subcommand=subcommand)

    args = parser.parse_args()

    log_level = {
        0: logging.INFO,
        1: logging.DEBUG,
    }.get(args.verbose, logging.DEBUG)
    log.setLevel(log_level)

    try:
        match getattr(args, "_subcommand", None):
            case SubCommand(_, cmd_handler, True):
                cmd_handler(**vars(args))
            case SubCommand(_, cmd_handler, False):
                cmd_handler(args)
            case None:
                parser.print_help()

    except CommandError as e:
        log.error(str(e))
        sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()
