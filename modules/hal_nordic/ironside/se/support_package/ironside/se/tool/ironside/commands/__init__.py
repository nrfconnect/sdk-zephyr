# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

from collections.abc import Sequence

from .. import SubCommand
from .check_periphconf import PERIPHCONF_CHECK_COMMAND
from .gen_uicr import GEN_UICR_COMMAND


def _load_ext_commands() -> Sequence[SubCommand]:
    from importlib.metadata import entry_points
    from itertools import chain

    ext_subcommands = entry_points(group="ironside.commands")

    return list(chain.from_iterable(ep.load() for ep in ext_subcommands))


SUBCOMMANDS = [GEN_UICR_COMMAND, PERIPHCONF_CHECK_COMMAND, *_load_ext_commands()]
