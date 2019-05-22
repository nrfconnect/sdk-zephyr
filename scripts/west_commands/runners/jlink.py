# Copyright (c) 2017 Linaro Limited.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for debugging with J-Link.'''

import os
import tempfile
import sys
import shutil

from west import log
from runners.core import ZephyrBinaryRunner, RunnerCaps, \
    BuildConfiguration

DEFAULT_JLINK_EXE = 'JLink.exe' if sys.platform == 'win32' else 'JLinkExe'
DEFAULT_JLINK_GDB_PORT = 2331


class JLinkBinaryRunner(ZephyrBinaryRunner):
    '''Runner front-end for the J-Link GDB server.'''

    def __init__(self, cfg, device,
                 commander=DEFAULT_JLINK_EXE,
                 flash_addr=0x0, erase=True,
                 iface='swd', speed='auto',
                 gdbserver='JLinkGDBServer', gdb_port=DEFAULT_JLINK_GDB_PORT,
                 tui=False):
        super(JLinkBinaryRunner, self).__init__(cfg)
        self.bin_name = cfg.bin_file
        self.elf_name = cfg.elf_file
        self.gdb_cmd = [cfg.gdb] if cfg.gdb else None
        self.device = device
        self.commander = commander
        self.flash_addr = flash_addr
        self.erase = erase
        self.gdbserver = gdbserver
        self.iface = iface
        self.speed = speed
        self.gdb_port = gdb_port
        self.tui_arg = ['-tui'] if tui else []

    @classmethod
    def name(cls):
        return 'jlink'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash', 'debug', 'debugserver', 'attach'},
                          flash_addr=True)

    @classmethod
    def do_add_parser(cls, parser):
        # Required:
        parser.add_argument('--device', required=True, help='device name')

        # Optional:
        parser.add_argument('--iface', default='swd',
                            help='interface to use, default is swd')
        parser.add_argument('--speed', default='auto',
                            help='interface speed, default is autodetect')
        parser.add_argument('--tui', default=False, action='store_true',
                            help='if given, GDB uses -tui')
        parser.add_argument('--gdbserver', default='JLinkGDBServer',
                            help='GDB server, default is JLinkGDBServer')
        parser.add_argument('--gdb-port', default=DEFAULT_JLINK_GDB_PORT,
                            help='pyocd gdb port, defaults to {}'.format(
                                DEFAULT_JLINK_GDB_PORT))
        parser.add_argument('--commander', default=DEFAULT_JLINK_EXE,
                            help='J-Link Commander, default is JLinkExe')
        parser.add_argument('--erase', default=False, action='store_true',
                            help='if given, mass erase flash before loading')

    @classmethod
    def create(cls, cfg, args):
        build_conf = BuildConfiguration(cfg.build_dir)
        flash_addr = cls.get_flash_address(args, build_conf)
        return JLinkBinaryRunner(cfg, args.device,
                                 commander=args.commander,
                                 flash_addr=flash_addr, erase=args.erase,
                                 iface=args.iface, speed=args.speed,
                                 gdbserver=args.gdbserver,
                                 gdb_port=args.gdb_port,
                                 tui=args.tui)

    def print_gdbserver_message(self):
        log.inf('J-Link GDB server running on port {}'.format(self.gdb_port))

    def check_cmd(self, cmd):
        if shutil.which(cmd) is None:
            log.die('{} is not installed or cannot be found'.format(cmd))

    def do_run(self, command, **kwargs):
        server_cmd = ([self.gdbserver] +
                      ['-select', 'usb', # only USB connections supported
                       '-port', str(self.gdb_port),
                       '-if', self.iface,
                       '-speed', self.speed,
                       '-device', self.device,
                       '-silent',
                       '-singlerun'])

        if command == 'flash':
            self.flash(**kwargs)
        elif command == 'debugserver':
            self.check_cmd(self.gdbserver)
            self.print_gdbserver_message()
            self.check_call(server_cmd)
        else:
            self.check_cmd(self.gdbserver)
            if self.gdb_cmd is None:
                raise ValueError('Cannot debug; gdb is missing')
            if self.elf_name is None:
                raise ValueError('Cannot debug; elf is missing')
            client_cmd = (self.gdb_cmd +
                          self.tui_arg +
                          [self.elf_name] +
                          ['-ex', 'target remote :{}'.format(self.gdb_port)])
            if command == 'debug':
                client_cmd += ['-ex', 'monitor halt',
                               '-ex', 'monitor reset',
                               '-ex', 'load']
            self.print_gdbserver_message()
            self.run_server_and_client(server_cmd, client_cmd)

    def flash(self, **kwargs):
        self.check_cmd(self.commander)
        if self.bin_name is None:
            raise ValueError('Cannot flash; bin_name is missing')

        lines = ['r'] # Reset and halt the target

        if self.erase:
            lines.append('erase') # Erase all flash sectors

        lines.append('loadfile {} 0x{:x}'.format(self.bin_name,
                                                 self.flash_addr))
        lines.append('g') # Start the CPU
        lines.append('q') # Close the connection and quit

        log.dbg('JLink commander script:')
        log.dbg('\n'.join(lines))

        # Don't use NamedTemporaryFile: the resulting file can't be
        # opened again on Windows.
        with tempfile.TemporaryDirectory(suffix='jlink') as d:
            fname = os.path.join(d, 'runner.jlink')
            with open(fname, 'wb') as f:
                f.writelines(bytes(line + '\n', 'utf-8') for line in lines)

            cmd = ([self.commander] +
                   ['-if', self.iface,
                    '-speed', self.speed,
                    '-device', self.device,
                    '-CommanderScript', fname])

            log.inf('Flashing Target Device')
            self.check_call(cmd)
