# Copyright (c) 2017 Linaro Limited.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for flashing with dfu-util.'''

from collections import namedtuple
import sys
import time

from runners.core import ZephyrBinaryRunner, RunnerCaps, \
    BuildConfiguration


DfuSeConfig = namedtuple('DfuSeConfig', ['address', 'options'])


class DfuUtilBinaryRunner(ZephyrBinaryRunner):
    '''Runner front-end for dfu-util.'''

    def __init__(self, cfg, dev_id, alt, img, exe='dfu-util',
                 dfuse_config=None):

        super().__init__(cfg)
        self.dev_id = dev_id # Used only for error checking in do_run
        self.alt = alt
        self.img = img
        self.cmd = [exe, '-d,{}'.format(dev_id)]
        try:
            self.list_pattern = ', alt={},'.format(int(self.alt))
        except ValueError:
            self.list_pattern = ', name="{}",'.format(self.alt)

        if dfuse_config is None:
            self.dfuse = False
        else:
            self.dfuse = True
        self.dfuse_config = dfuse_config
        self.reset = False

    @classmethod
    def name(cls):
        return 'dfu-util'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash'}, dev_id=True, flash_addr=True)

    @classmethod
    def dev_id_help(cls) -> str:
        return 'USB VID:PID of the connected device.'

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument("--alt", required=True,
                            help="interface alternate setting number or name")

        # Optional:
        parser.add_argument("--pid", dest='dev_id',
                            help=cls.dev_id_help())
        parser.add_argument("--img",
                            help="binary to flash, default is --bin-file")
        parser.add_argument("--dfuse", default=False, action='store_true',
                            help='''use the DfuSe protocol extensions
                                 supported by STMicroelectronics
                                 devices (if given, the image flash
                                 address respects
                                 CONFIG_FLASH_BASE_ADDRESS and
                                 CONFIG_FLASH_LOAD_OFFSET)''')
        parser.add_argument("--dfuse-modifiers", default='leave',
                            help='''colon-separated list of additional
                                 DfuSe modifiers for dfu-util's -s
                                 option (default is
                                 "-s <flash-address>:leave", which starts
                                 execution immediately); requires
                                 --dfuse
                                 ''')
        parser.add_argument('--dfu-util', default='dfu-util',
                            help='dfu-util executable; defaults to "dfu-util"')

    @classmethod
    def do_create(cls, cfg, args):
        if args.img is None:
            args.img = cfg.bin_file

        if args.dfuse:
            args.dt_flash = True  # --dfuse implies --dt-flash.
            build_conf = BuildConfiguration(cfg.build_dir)
            dcfg = DfuSeConfig(address=cls.get_flash_address(args, build_conf),
                               options=args.dfuse_modifiers)
        else:
            dcfg = None

        ret = DfuUtilBinaryRunner(cfg, args.dev_id, args.alt, args.img,
                                  exe=args.dfu_util, dfuse_config=dcfg)
        ret.ensure_device()
        return ret

    def ensure_device(self):
        if not self.find_device():
            self.reset = True
            print('Please reset your board to switch to DFU mode...')
            while not self.find_device():
                time.sleep(0.1)

    def find_device(self):
        cmd = list(self.cmd) + ['-l']
        output = self.check_output(cmd)
        output = output.decode(sys.getdefaultencoding())
        return self.list_pattern in output

    def do_run(self, command, **kwargs):
        if not self.dev_id:
            raise RuntimeError('Please specify a USB VID:PID with the '
                               '-i/--dev-id or --pid command-line switch.')
        self.require(self.cmd[0])
        self.ensure_output('bin')

        if not self.find_device():
            raise RuntimeError('device not found')

        cmd = list(self.cmd)
        if self.dfuse:
            # http://dfu-util.sourceforge.net/dfuse.html
            dcfg = self.dfuse_config
            addr_opts = hex(dcfg.address) + ':' + dcfg.options
            cmd.extend(['-s', addr_opts])
        cmd.extend(['-a', self.alt, '-D', self.img])
        self.check_call(cmd)

        if self.dfuse and 'leave' in dcfg.options.split(':'):
            # Normal DFU devices generally need to be reset to switch
            # back to the flashed program.
            #
            # DfuSe targets do as well, except when 'leave' is given
            # as an option.
            self.reset = False
        if self.reset:
            print('Now reset your board again to switch back to runtime mode.')
