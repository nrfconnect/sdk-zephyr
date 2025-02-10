# Copyright (c) 2017 Linaro Limited.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner stub for QEMU.'''

from runners.core import RunnerCaps, ZephyrBinaryRunner


class QemuBinaryRunner(ZephyrBinaryRunner):
    '''Place-holder for QEMU runner customizations.'''

    @classmethod
    def name(cls):
        return 'qemu'

    @classmethod
    def capabilities(cls):
        # This is a stub.
        return RunnerCaps(commands=set())

    @classmethod
    def do_add_parser(cls, parser):
        pass                    # Nothing to do.

    @classmethod
    def do_create(cls, cfg, args):
        return QemuBinaryRunner(cfg)

    def do_run(self, command, **kwargs):
        pass
