# Copyright (c) 2026 Nordic Semiconductor ASA.
#
# SPDX-License-Identifier: Apache-2.0

'''Direct PyLink runner for nRF7120 engineering-silicon PDKs.'''

import os
import time

from runners.core import RunnerCaps, ZephyrBinaryRunner

try:
    from intelhex import IntelHex
except ImportError:
    IntelHex = None

try:
    import pylink
except ImportError:
    pylink = None


CTRL_AP = 2
CTRL_AP_RESET = 0x000
CTRL_AP_ERASEALL = 0x004
CTRL_AP_ERASEALLSTATUS = 0x008
CTRL_AP_BOOTSTATUS = 0x038

ERASEALL_READY = 0
ERASEALL_READY_TO_RESET = 1
ERASEALL_BUSY = 2
ERASEALL_ERROR = 3
CTRL_AP_HARD_RESET = 2
LCS_TEST = 0x18888

MRAMC_WEN = 0x5004E500
MRAMC_WAITSTATES = 0x5004E508
MRAMC_CONFIGNVR0 = 0x5004E580
MRAMC_WEN_DISABLE = 0
MRAMC_WEN_DIRECT_WRITE = 2
MRAMC_CONFIGNVR_DISABLE = 0
MRAMC_CONFIGNVR_WRITE_ERASE = 22
MRAMC_WAITSTATES_KEY = 0xA66D

CPUCONF_CPUSTART = 0x50073508
CPUCONF_CPUWAIT = 0x5007350C
SCB_VTOR = 0xE000ED08
DHCSR = 0xE000EDF0
DCRSR = 0xE000EDF4
DCRDR = 0xE000EDF8
DEMCR = 0xE000EDFC

DHCSR_HALT = 0xA05F0003
DHCSR_RESUME = 0xA05F0001
DEMCR_VC_CORERESET = 0x00000001
DCRSR_WRITE = 0x00010000
REGSEL_PC = 0x0F
REGSEL_XPSR = 0x10
REGSEL_MSP = 0x11
XPSR_THUMB = 0x01000000
SRAM_BASE = 0x20000000
SRAM_MASK = 0xFFF00000


class Nrf7120PdkBinaryRunner(ZephyrBinaryRunner):
    '''Erase, program, verify, and start an nRF7120 PDK through PyLink.'''

    def __init__(self, cfg, dev_id=None, speed=1000, erase_timeout=10,
                 mram_waitstates=6, reset=True, dry_run=False, hex_files=None,
                 startup=None, remote_jlink=None, tunnel_port=19020,
                 remote_ip=None):
        super().__init__(cfg)
        self.dev_id = dev_id
        self.speed = speed
        self.erase_timeout = erase_timeout
        self.mram_waitstates = mram_waitstates
        self.finish = bool(reset)
        self.dry_run = dry_run
        self.hex_files = list(hex_files or [])
        self.startup = startup
        self.remote_jlink = remote_jlink
        self.tunnel_port = tunnel_port
        self.remote_ip = remote_ip

    @classmethod
    def name(cls):
        return 'nrf7120-pdk'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash'}, dev_id=True, reset=True,
                          dry_run=True)

    @classmethod
    def dev_id_help(cls):
        return ('J-Link serial number; if omitted, exactly one connected '
                'J-Link is selected automatically')

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--speed', type=int, default=1000,
                            help='SWD speed in kHz (default: 1000)')
        parser.add_argument('--erase-timeout', type=float, default=10,
                            help='CTRL-AP ERASEALL timeout in seconds')
        parser.add_argument('--mram-waitstates',
                            type=lambda value: int(value, 0), default=6,
                            help='MRAMC.WAITSTATENUM before start (default: 6)')
        parser.add_argument('--remote-jlink', metavar='SERIAL',
                            help='connect to a remote J-Link over the SEGGER '
                                 'tunnel server (J-Link Remote Server) using '
                                 'the given tunnel serial number, mirroring '
                                 "nrfutil's --remote-jlink option")
        parser.add_argument('--tunnel-port', type=int, default=19020,
                            help='tunnel server port for --remote-jlink '
                                 '(default: 19020)')
        parser.add_argument('--remote-ip', metavar='HOST[:PORT]',
                            help='connect to a J-Link Remote Server reachable '
                                 'by IP (default port 19020), e.g. '
                                 '192.168.1.10 or 192.168.1.10:19020')
        parser.set_defaults(reset=True)

    @classmethod
    def do_create(cls, cfg, args):
        return cls(cfg, args.dev_id, speed=args.speed,
                   erase_timeout=args.erase_timeout,
                   mram_waitstates=args.mram_waitstates, reset=args.reset,
                   dry_run=args.dry_run,
                   hex_files=getattr(args, 'nrf7120_pdk_hex_files', None),
                   startup=getattr(args, 'nrf7120_pdk_startup', None),
                   remote_jlink=args.remote_jlink,
                   tunnel_port=args.tunnel_port,
                   remote_ip=args.remote_ip)

    @classmethod
    def args_from_previous_runner(cls, previous_runner, args):
        if args.dev_id is None:
            args.dev_id = previous_runner.dev_id
        args.nrf7120_pdk_hex_files = previous_runner.hex_files
        args.nrf7120_pdk_startup = previous_runner.startup

    @staticmethod
    def _word(image, address):
        data = image.tobinarray(start=address, size=4)
        return int.from_bytes(bytes(data), 'little')

    def _validate_and_collect(self):
        if not self.build_conf.getboolean('CONFIG_SOC_NRF7120_ENGA_CPUAPP'):
            raise RuntimeError(
                'the nrf7120-pdk runner requires an nRF7120 '
                'engineering-silicon CPUAPP build')

        hex_file = self.cfg.hex_file
        if not hex_file or not os.path.isfile(hex_file):
            raise RuntimeError(f'HEX file not found: {hex_file}')
        if hex_file not in self.hex_files:
            self.hex_files.append(hex_file)

        if self.startup is not None:
            return
        if IntelHex is None:
            raise RuntimeError('Python dependency intelhex is required')

        vtor = (self.build_conf.get('CONFIG_FLASH_BASE_ADDRESS', 0) +
                self.build_conf.get('CONFIG_FLASH_LOAD_OFFSET', 0))
        if vtor == 0:
            return

        image = IntelHex()
        image.loadfile(hex_file, format='hex')
        sp = self._word(image, vtor)
        pc = self._word(image, vtor + 4)
        if (sp & SRAM_MASK) == SRAM_BASE and pc & 1:
            self.startup = (vtor, sp, pc & ~1)

    def _select_probe(self, jlink):
        if self.remote_ip is not None:
            return None
        if self.remote_jlink is not None:
            return int(self.remote_jlink)
        if self.dev_id is not None:
            return int(self.dev_id)

        probes = jlink.connected_emulators()
        if not probes:
            raise RuntimeError('no connected J-Link probes found')
        if len(probes) != 1:
            serials = ', '.join(str(probe.SerialNumber) for probe in probes)
            raise RuntimeError(
                f'multiple J-Link probes found ({serials}); use --dev-id')
        self.dev_id = str(probes[0].SerialNumber)
        return probes[0].SerialNumber

    @staticmethod
    def _select_ap_bank(jlink, ap, address):
        select = (ap << 24) | (address & 0xF0)
        jlink.coresight_write(reg=2, data=select, ap=False)

    def _ap_read(self, jlink, ap, address):
        self._select_ap_bank(jlink, ap, address)
        return jlink.coresight_read(reg=(address & 0x0C) // 4, ap=True)

    def _ap_write(self, jlink, ap, address, value):
        self._select_ap_bank(jlink, ap, address)
        jlink.coresight_write(reg=(address & 0x0C) // 4,
                             data=value, ap=True)

    def _require_test_lcs(self, jlink):
        deadline = time.monotonic() + self.erase_timeout
        while True:
            bootstatus = self._ap_read(jlink, CTRL_AP, CTRL_AP_BOOTSTATUS)
            lcs = (bootstatus >> 12) & 0xFFFFF
            if lcs == LCS_TEST:
                return
            if lcs not in (0, 0x3DBEE) or time.monotonic() >= deadline:
                raise RuntimeError(
                    f'nRF7120 must already be in TEST LCS; '
                    f'CTRL-AP BOOTSTATUS={bootstatus:#010x}, LCS={lcs:#07x}')
            time.sleep(0.01)

    def _erase_all(self, jlink):
        self._ap_write(jlink, CTRL_AP, CTRL_AP_ERASEALL, 1)
        deadline = time.monotonic() + self.erase_timeout
        while True:
            status = (self._ap_read(
                jlink, CTRL_AP, CTRL_AP_ERASEALLSTATUS) & 0x3)
            if status != ERASEALL_BUSY:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError('nRF7120 CTRL-AP ERASEALL timed out')
            time.sleep(0.1)

        if status == ERASEALL_READY_TO_RESET:
            self._ap_write(
                jlink, CTRL_AP, CTRL_AP_RESET, CTRL_AP_HARD_RESET)
            time.sleep(0.1)
        elif status != ERASEALL_READY:
            name = 'error' if status == ERASEALL_ERROR else f'status {status}'
            raise RuntimeError(f'nRF7120 CTRL-AP ERASEALL failed: {name}')

    @staticmethod
    def _aligned_region(jlink, start, data):
        end = start + len(data)
        aligned_start = start & ~0xF
        aligned_end = (end + 0xF) & ~0xF
        prefix = list(jlink.memory_read8(aligned_start,
                                         start - aligned_start))
        suffix = list(jlink.memory_read8(end, aligned_end - end))
        return aligned_start, bytes(prefix) + bytes(data) + bytes(suffix)

    def _program_hex(self, jlink, hex_file):
        image = IntelHex()
        image.loadfile(hex_file, format='hex')
        self.logger.info(f'Programming and verifying {hex_file}')

        for start, end in image.segments():
            original = bytes(image.tobinarray(start=start, end=end - 1))
            aligned_start, data = self._aligned_region(
                jlink, start, original)
            words = [
                int.from_bytes(data[index:index + 4], 'little')
                for index in range(0, len(data), 4)
            ]
            jlink.memory_write(aligned_start, words, nbits=32)
            jlink.memory_write32(aligned_start, [words[0]])

            actual = bytes(jlink.memory_read8(start, len(original)))
            if actual != original:
                raise RuntimeError(
                    f'verification failed for {hex_file} at {start:#010x}')

    def _start_cpu(self, jlink):
        if self.startup is None:
            raise RuntimeError(
                'no valid application vector table was found; '
                'CONFIG_FLASH_LOAD_OFFSET must be nonzero')

        vtor, sp, pc = self.startup
        waitstates = (MRAMC_WAITSTATES_KEY << 16) | self.mram_waitstates
        writes = [
            (CPUCONF_CPUSTART, 1),
            (DHCSR, DHCSR_HALT),
            (DEMCR, DEMCR_VC_CORERESET),
            (CPUCONF_CPUWAIT, 0),
            (MRAMC_WAITSTATES, waitstates),
            (SCB_VTOR, vtor),
            (DCRDR, sp),
            (DCRSR, DCRSR_WRITE | REGSEL_MSP),
            (DCRDR, pc),
            (DCRSR, DCRSR_WRITE | REGSEL_PC),
            (DCRDR, XPSR_THUMB),
            (DCRSR, DCRSR_WRITE | REGSEL_XPSR),
            (DEMCR, 0),
            (DHCSR, DHCSR_RESUME),
        ]
        for address, value in writes:
            jlink.memory_write32(address, [value])

    def _flash(self):
        if pylink is None:
            raise RuntimeError(
                'Python dependency pylink-square is required; '
                'install it with pip')
        if IntelHex is None:
            raise RuntimeError('Python dependency intelhex is required')

        if self.remote_jlink is not None and self.remote_ip is not None:
            raise RuntimeError(
                '--remote-jlink and --remote-ip are mutually exclusive')

        jlink = pylink.JLink()
        serial = self._select_probe(jlink)
        mram_enabled = False
        try:
            if self.remote_ip is not None:
                ip_addr = self.remote_ip
                if ':' not in ip_addr:
                    ip_addr = f'{ip_addr}:19020'
                self.logger.info(
                    f'Connecting to J-Link Remote Server at {ip_addr}')
                jlink.open(ip_addr=ip_addr)
            elif self.remote_jlink is not None:
                self.logger.info(
                    f'Connecting to remote J-Link {serial} over tunnel '
                    f'server (port {self.tunnel_port})')
                jlink.open_tunnel(serial, port=self.tunnel_port)
            else:
                jlink.open(serial_no=serial)
            jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
            jlink.coresight_configure()
            jlink.exec_command('CORESIGHT_SetIndexAHBAPToUse=0')
            jlink.exec_command('SetRestartOnClose=0')

            self._require_test_lcs(jlink)
            self._erase_all(jlink)

            jlink.connect('CORTEX-M33', speed=self.speed, verbose=False)
            jlink.halt()
            jlink.memory_write8(
                MRAMC_CONFIGNVR0, [MRAMC_CONFIGNVR_WRITE_ERASE])
            jlink.memory_write32(MRAMC_WEN, [MRAMC_WEN_DIRECT_WRITE])
            mram_enabled = True

            for hex_file in self.hex_files:
                self._program_hex(jlink, hex_file)

            jlink.memory_write8(
                MRAMC_CONFIGNVR0, [MRAMC_CONFIGNVR_DISABLE])
            jlink.memory_write32(MRAMC_WEN, [MRAMC_WEN_DISABLE])
            mram_enabled = False
            self._start_cpu(jlink)
        finally:
            if jlink.opened():
                try:
                    if mram_enabled:
                        jlink.memory_write8(
                            MRAMC_CONFIGNVR0, [MRAMC_CONFIGNVR_DISABLE])
                        jlink.memory_write32(
                            MRAMC_WEN, [MRAMC_WEN_DISABLE])
                finally:
                    jlink.close()

    def do_run(self, command, **kwargs):
        if command != 'flash':
            raise ValueError(f'unsupported command: {command}')
        self._validate_and_collect()

        # Sysbuild supplies --reset only to the final domain for this runner.
        if not self.finish:
            return

        if self.dry_run:
            self.logger.info(
                f'Would program {len(self.hex_files)} image(s) through PyLink')
            return
        self._flash()
