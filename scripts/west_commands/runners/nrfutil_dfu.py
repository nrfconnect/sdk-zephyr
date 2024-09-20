# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

from runners.core import ZephyrBinaryRunner, RunnerCaps
import subprocess

class NrfutilDfuRunner(ZephyrBinaryRunner):
    """Runner for dfu-programming devices with nrfutil."""

    def __init__(self, cfg, dev_id):
        super(NrfutilDfuRunner, self).__init__(cfg)
        self.cfg = cfg
        self.family = None
        self.dev_id = dev_id

    @classmethod
    def do_create(cls, cfg, args):
        return NrfutilDfuRunner(cfg, args.dev_id)

    @classmethod
    def name(cls):
        return 'nrfutil_dfu'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash'}, dev_id=True)

    @classmethod
    def do_add_parser(cls, parser):
        pass

    def ensure_family(self):
        # Ensure self.family is set.

        if self.family is not None:
            return

        if self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF51X'):
            self.family = 'nrf51'
        elif self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF52X'):
            self.family = 'nrf52'
        elif self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF53X'):
            self.family = 'nrf53'
        elif self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF54LX'):
            self.family = 'nrf54l'
        elif self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF54HX'):
            self.family = 'nrf54h'
        elif self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF91X'):
            self.family = 'nrf91'
        elif self.build_conf.getboolean('CONFIG_SOC_SERIES_NRF92X'):
            self.family = 'nrf92'
        else:
            raise RuntimeError(f'unknown nRF; update {__file__}')

    def do_run(self, command, **kwargs):
        self.require('nrfutil')

        self.ensure_output('zip')

        self.ensure_family()

        if self.build_conf.getboolean('CONFIG_SOC_NRF5340_CPUNET'):
            self.logger.info('skipping DFU for network core, ' + \
                'this is done as part of the application core DFU')
            return

        cmd = ['nrfutil', 'device', 'program',
               '--x-family', self.family,
               '--firmware', self.cfg.zip_file]
        if self.dev_id:
            cmd += ['--serial-number', self.dev_id]

        subprocess.Popen(cmd).wait()


