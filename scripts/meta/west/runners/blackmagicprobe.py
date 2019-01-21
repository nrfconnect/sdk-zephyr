# Copyright (c) 2018 Roman Tataurov <diytronic@yandex.ru>
# Modified 2018 Tavish Naruka <tavishnaruka@gmail.com>
#
# SPDX-License-Identifier: Apache-2.0
'''Runner for flashing with Black Magic Probe.'''
# https://github.com/blacksphere/blackmagic/wiki

from west.runners.core import ZephyrBinaryRunner, RunnerCaps


class BlackMagicProbeRunner(ZephyrBinaryRunner):
    '''Runner front-end for Black Magic probe.'''

    def __init__(self, cfg, gdb_serial):
        super(BlackMagicProbeRunner, self).__init__(cfg)
        self.gdb = [cfg.gdb] if cfg.gdb else None
        self.elf_file = cfg.elf_file
        self.gdb_serial = gdb_serial

    @classmethod
    def name(cls):
        return 'blackmagicprobe'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash', 'debug', 'attach'})

    @classmethod
    def create(cls, cfg, args):
        return BlackMagicProbeRunner(cfg, args.gdb_serial)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--gdb-serial', default='/dev/ttyACM0',
                            help='GDB serial port')

    def bmp_flash(self, command, **kwargs):
        if self.gdb is None:
            raise ValueError('Cannot flash; gdb is missing')
        if self.elf_file is None:
            raise ValueError('Cannot debug; elf file is missing')
        command = (self.gdb +
                   ['-ex', "set confirm off",
                    '-ex', "target extended-remote {}".format(self.gdb_serial),
                    '-ex', "monitor swdp_scan",
                    '-ex', "attach 1",
                    '-ex', "load {}".format(self.elf_file),
                    '-ex', "kill",
                    '-ex', "quit",
                    '-silent'])
        self.check_call(command)

    def bmp_attach(self, command, **kwargs):
        if self.gdb is None:
            raise ValueError('Cannot attach; gdb is missing')
        if self.elf_file is None:
            command = (self.gdb +
                       ['-ex', "set confirm off",
                        '-ex', "target extended-remote {}".format(
                            self.gdb_serial),
                        '-ex', "monitor swdp_scan",
                        '-ex', "attach 1"])
        else:
            command = (self.gdb +
                       ['-ex', "set confirm off",
                        '-ex', "target extended-remote {}".format(
                            self.gdb_serial),
                        '-ex', "monitor swdp_scan",
                        '-ex', "attach 1",
                        '-ex', "file {}".format(self.elf_file)])
        self.check_call(command)

    def bmp_debug(self, command, **kwargs):
        if self.gdb is None:
            raise ValueError('Cannot debug; gdb is missing')
        if self.elf_file is None:
            raise ValueError('Cannot debug; elf file is missing')
        command = (self.gdb +
                   ['-ex', "set confirm off",
                    '-ex', "target extended-remote {}".format(self.gdb_serial),
                    '-ex', "monitor swdp_scan",
                    '-ex', "attach 1",
                    '-ex', "file {}".format(self.elf_file),
                    '-ex', "load {}".format(self.elf_file)])
        self.check_call(command)

    def do_run(self, command, **kwargs):

        if command == 'flash':
            self.bmp_flash(command, **kwargs)
        elif command == 'debug':
            self.bmp_debug(command, **kwargs)
        elif command == 'attach':
            self.bmp_attach(command, **kwargs)
        else:
            self.bmp_flash(command, **kwargs)
