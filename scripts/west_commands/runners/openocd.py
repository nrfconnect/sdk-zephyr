# Copyright (c) 2017 Linaro Limited.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for openocd.'''

from os import path

from runners.core import ZephyrBinaryRunner

DEFAULT_OPENOCD_TCL_PORT = 6333
DEFAULT_OPENOCD_TELNET_PORT = 4444
DEFAULT_OPENOCD_GDB_PORT = 3333


class OpenOcdBinaryRunner(ZephyrBinaryRunner):
    '''Runner front-end for openocd.'''

    def __init__(self, cfg, pre_init=None, pre_load=None,
                 load_cmd=None, verify_cmd=None, post_verify=None,
                 tui=None, config=None,
                 tcl_port=DEFAULT_OPENOCD_TCL_PORT,
                 telnet_port=DEFAULT_OPENOCD_TELNET_PORT,
                 gdb_port=DEFAULT_OPENOCD_GDB_PORT):
        super(OpenOcdBinaryRunner, self).__init__(cfg)

        if not config:
            default = path.join(cfg.board_dir, 'support', 'openocd.cfg')
            if path.exists(default):
                config = default
        self.openocd_config = config

        if path.exists(self.openocd_config):
            search_args = ['-s', path.dirname(self.openocd_config)]
        else:
            search_args = []

        if cfg.openocd_search is not None:
            search_args.extend(['-s', cfg.openocd_search])
        self.openocd_cmd = [cfg.openocd] + search_args
        self.hex_name = cfg.hex_file
        self.elf_name = cfg.elf_file
        self.pre_init = pre_init or []
        self.pre_load = pre_load or []
        self.load_cmd = load_cmd
        self.verify_cmd = verify_cmd
        self.post_verify = post_verify or []
        self.tcl_port = tcl_port
        self.telnet_port = telnet_port
        self.gdb_port = gdb_port
        self.gdb_cmd = [cfg.gdb] if cfg.gdb else None
        self.tui_arg = ['-tui'] if tui else []

    @classmethod
    def name(cls):
        return 'openocd'

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--config',
                            help='if given, override default config file')
        # Options for flashing:
        parser.add_argument('--cmd-pre-init', action='append',
                            help='''Command to run before calling init;
                            may be given multiple times''')
        parser.add_argument('--cmd-pre-load', action='append',
                            help='''Command to run before flashing;
                            may be given multiple times''')
        parser.add_argument('--cmd-load',
                            help='''Command to load/flash binary
                            (required when flashing)''')
        parser.add_argument('--cmd-verify',
                            help='''Command to verify flashed binary''')
        parser.add_argument('--cmd-post-verify', action='append',
                            help='''Command to run after verification;
                            may be given multiple times''')

        # Options for debugging:
        parser.add_argument('--tui', default=False, action='store_true',
                            help='if given, GDB uses -tui')
        parser.add_argument('--tcl-port', default=DEFAULT_OPENOCD_TCL_PORT,
                            help='openocd TCL port, defaults to 6333')
        parser.add_argument('--telnet-port',
                            default=DEFAULT_OPENOCD_TELNET_PORT,
                            help='openocd telnet port, defaults to 4444')
        parser.add_argument('--gdb-port', default=DEFAULT_OPENOCD_GDB_PORT,
                            help='openocd gdb port, defaults to 3333')

    @classmethod
    def create(cls, cfg, args):
        return OpenOcdBinaryRunner(
            cfg,
            pre_init=args.cmd_pre_init,
            pre_load=args.cmd_pre_load, load_cmd=args.cmd_load,
            verify_cmd=args.cmd_verify, post_verify=args.cmd_post_verify,
            tui=args.tui, config=args.config,
            tcl_port=args.tcl_port, telnet_port=args.telnet_port,
            gdb_port=args.gdb_port)

    def do_run(self, command, **kwargs):
        self.require(self.openocd_cmd[0])

        self.cfg_cmd = []
        if self.openocd_config is not None:
            self.cfg_cmd = ['-f', self.openocd_config]

        if command == 'flash':
            self.do_flash(**kwargs)
        elif command == 'debug':
            self.do_debug(**kwargs)
        else:
            self.do_debugserver(**kwargs)

    def do_flash(self, **kwargs):
        if not path.isfile(self.hex_name):
            raise ValueError('Cannot flash; hex file ({}) does not exist. '.
                             format(self.hex_name) +
                             'Try enabling CONFIG_BUILD_OUTPUT_HEX.')
        if self.load_cmd is None:
            raise ValueError('Cannot flash; load command is missing')
        if self.verify_cmd is None:
            raise ValueError('Cannot flash; verify command is missing')

        self.logger.info('Flashing file: {}'.format(self.hex_name))

        pre_init_cmd = []
        pre_load_cmd = []
        post_verify_cmd = []
        for i in self.pre_init:
            pre_init_cmd.append("-c")
            pre_init_cmd.append(i)

        for i in self.pre_load:
            pre_load_cmd.append("-c")
            pre_load_cmd.append(i)

        for i in self.post_verify:
            post_verify_cmd.append("-c")
            post_verify_cmd.append(i)

        cmd = (self.openocd_cmd + self.cfg_cmd +
               pre_init_cmd + ['-c', 'init',
                                '-c', 'targets'] +
               pre_load_cmd + ['-c', 'reset halt',
                                '-c', self.load_cmd + ' ' + self.hex_name,
                                '-c', 'reset halt'] +
               ['-c', self.verify_cmd + ' ' + self.hex_name] +
               post_verify_cmd +
               ['-c', 'reset run',
                '-c', 'shutdown'])
        self.check_call(cmd)

    def do_debug(self, **kwargs):
        if self.gdb_cmd is None:
            raise ValueError('Cannot debug; no gdb specified')
        if self.elf_name is None:
            raise ValueError('Cannot debug; no .elf specified')

        pre_init_cmd = []
        for i in self.pre_init:
            pre_init_cmd.append("-c")
            pre_init_cmd.append(i)

        server_cmd = (self.openocd_cmd + self.cfg_cmd +
                      ['-c', 'tcl_port {}'.format(self.tcl_port),
                       '-c', 'telnet_port {}'.format(self.telnet_port),
                       '-c', 'gdb_port {}'.format(self.gdb_port)] +
                      pre_init_cmd + ['-c', 'init',
                                       '-c', 'targets',
                                       '-c', 'halt'])
        gdb_cmd = (self.gdb_cmd + self.tui_arg +
                   ['-ex', 'target remote :{}'.format(self.gdb_port),
                    self.elf_name])
        self.require(gdb_cmd[0])
        self.run_server_and_client(server_cmd, gdb_cmd)

    def do_debugserver(self, **kwargs):
        pre_init_cmd = []
        for i in self.pre_init:
            pre_init_cmd.append("-c")
            pre_init_cmd.append(i)

        cmd = (self.openocd_cmd + self.cfg_cmd +
               ['-c', 'tcl_port {}'.format(self.tcl_port),
                '-c', 'telnet_port {}'.format(self.telnet_port),
                '-c', 'gdb_port {}'.format(self.gdb_port)] +
               pre_init_cmd + ['-c', 'init',
                                '-c', 'targets',
                                '-c', 'reset halt'])
        self.check_call(cmd)
