# Copyright (c) 2023 Nordic Semiconductor ASA.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for flashing with nrfutil.'''

import json
import subprocess
import sys
from pathlib import Path

from runners.core import _DRY_RUN
from runners.nrf_common import NrfBinaryRunner


class NrfUtilBinaryRunner(NrfBinaryRunner):
    '''Runner front-end for nrfutil.'''

    def __init__(self, cfg, family, softreset, pinreset, dev_id, erase=False,
                 erase_mode=None, ext_erase_mode=None, reset=True, tool_opt=None,
                 force=False, recover=False, suit_starter=False,
                 ext_mem_config_file=None, ncs_provision=False):

        super().__init__(cfg, family, softreset, pinreset, dev_id, erase,
                         erase_mode, ext_erase_mode, reset, tool_opt, force,
                         recover)

        self.suit_starter = suit_starter
        self.ext_mem_config_file = ext_mem_config_file
        self.ncs_provision = ncs_provision

        self._ops = []
        self._op_id = 1

    @classmethod
    def name(cls):
        return 'nrfutil'

    @classmethod
    def capabilities(cls):
        return NrfBinaryRunner._capabilities(mult_dev_ids=True)

    @classmethod
    def dev_id_help(cls) -> str:
        return NrfBinaryRunner._dev_id_help() + \
               '''.\n This option can be specified multiple times'''

    @classmethod
    def tool_opt_help(cls) -> str:
        return 'Additional options for nrfutil, e.g. "--log-level"'

    @classmethod
    def do_create(cls, cfg, args):
        return NrfUtilBinaryRunner(cfg, args.nrf_family, args.softreset,
                                   args.pinreset, args.dev_id, erase=args.erase,
                                   erase_mode=args.erase_mode,
                                   ext_erase_mode=args.ext_erase_mode,
                                   reset=args.reset, tool_opt=args.tool_opt,
                                   force=args.force, recover=args.recover,
                                   suit_starter=args.suit_manifest_starter,
                                   ext_mem_config_file=args.ext_mem_config_file,
                                   ncs_provision=args.ncs_provision)

    @classmethod
    def do_add_parser(cls, parser):
        super().do_add_parser(parser)
        parser.add_argument('--suit-manifest-starter', required=False,
                            action='store_true',
                            help='Use the SUIT manifest starter file')
        parser.add_argument('--ext-mem-config-file', required=False,
                            dest='ext_mem_config_file',
                            help='path to an JSON file with external memory configuration')
        parser.add_argument('--ncs-provision',
                            action='store_true',
                            help='run ncs-provision with using default keys')

    def _exec(self, args):
        jout_all = []

        cmd = ['nrfutil', '--json', 'device'] + args
        self._log_cmd(cmd)

        if _DRY_RUN:
            return {}

        with subprocess.Popen(cmd, stdout=subprocess.PIPE) as p:
            for line in iter(p.stdout.readline, b''):
                # https://github.com/ndjson/ndjson-spec
                jout = json.loads(line.decode(sys.getdefaultencoding()))
                jout_all.append(jout)

                if 'x-execute-batch' in args:
                    if jout['type'] == 'batch_update':
                        pld = jout['data']['data']
                        if (
                            pld['type'] == 'task_progress' and
                            pld['data']['progress']['progressPercentage'] == 0
                        ):
                            self.logger.info(pld['data']['progress']['description'])
                    elif jout['type'] == 'batch_end' and jout['data']['error']:
                        raise subprocess.CalledProcessError(
                            jout['data']['error']['code'], cmd
                        )

        return jout_all

    def do_get_boards(self):
        out = self._exec(['list'])
        devs = []
        for o in out:
            if o['type'] == 'task_end':
                devs = o['data']['data']['devices']
        snrs = [dev['serialNumber'] for dev in devs if dev['traits']['jlink']]

        self.logger.debug(f'Found boards: {snrs}')
        return snrs

    def do_require(self):
        self.require('nrfutil')

    def _generate_ncs_provision_key_file(
        self,
        keys: list[str] | str,
        keyname: str,  # UROT_PUBKEY, BL_PUBKEY, APP_PUBKEY
        output_file: Path
    ):
        """Generate a key file for ncs-provision.
        Currently uses the west ncs-provision command to generate the JSON file.
        Consider importing directly from sdk-nrf/scripts/west_commands/ncs_provision.py
        or sdk-nrf/scripts/generate_psa_key_attributes.py to call methods directly
        """
        build_dir = Path(self.cfg.build_dir).parent
        ncs_keyfile = build_dir / 'keyfile.json'
        if ncs_keyfile.exists():
            ncs_keyfile.unlink()
        command = [
            'west', 'ncs-provision', 'upload',
            '--soc', 'nrf54l15',
            '--keyname', keyname,
            '--build-dir', str(build_dir),
            '--dry-run'
        ]
        for key in keys:
            command += ["--key", key]
        self.check_call(command)

        # move the generated ncs keyfile to the output_file
        if output_file.exists():
            output_file.unlink()
        ncs_keyfile.rename(output_file)

    def _ncs_provision_for_nsib(self):
        if not self.sysbuild_conf.getboolean('SB_CONFIG_SECURE_BOOT_SIGNATURE_TYPE_ED25519'):
            return
        build_dir = Path(self.cfg.build_dir).parent
        key_file = self.sysbuild_conf.get('SB_CONFIG_SECURE_BOOT_SIGNING_KEY_FILE') or str(
            build_dir / 'GENERATED_NON_SECURE_SIGN_KEY_PRIVATE.pem')
        if not Path(key_file).exists():
            raise RuntimeError(f'Key file {key_file} does not exist')

        ncs_keyfile = build_dir / 'keyfile_for_nsib.json'
        self._generate_ncs_provision_key_file(
            keys=[key_file],
            keyname='BL_PUBKEY',
            output_file=ncs_keyfile
        )
        self.exec_op('x-provision-keys', keyfile=str(ncs_keyfile))

    def _ncs_provision_for_mcuboot(self):
        if not self.sysbuild_conf.getboolean('SB_CONFIG_MCUBOOT_SIGNATURE_USING_KMU'):
            return
        key_file = self.sysbuild_conf.get('SB_CONFIG_BOOT_SIGNATURE_KEY_FILE')
        if not Path(key_file).exists():
            raise RuntimeError(f'Key file {key_file} does not exist')

        ncs_keyfile = Path(self.cfg.build_dir).parent / 'keyfile_for_mcuboot.json'
        self._generate_ncs_provision_key_file(
            keys=[key_file],
            keyname='UROT_PUBKEY',
            output_file=ncs_keyfile
        )
        self.exec_op('x-provision-keys', keyfile=str(ncs_keyfile))

    def do_ncs_provision(self):
        if not self.ncs_provision:
            return
        self._ncs_provision_for_nsib()
        self._ncs_provision_for_mcuboot()

    def _insert_op(self, op):
        op['operationId'] = f'{self._op_id}'
        self._op_id += 1
        self._ops.append(op)

    def _format_dev_ids(self):
        if isinstance(self.dev_id, list):
            return ','.join(self.dev_id)
        else:
            return self.dev_id

    def _append_batch(self, op, json_file):
        _op = op['operation']
        op_type = _op['type']

        cmd = [f'{op_type}']

        if op_type == 'program':
            cmd += ['--firmware', _op['firmware']['file']]
            opts = _op['options']
            # populate the options
            cmd.append('--options')
            cli_opts = f"chip_erase_mode={opts['chip_erase_mode']}"
            if opts.get('ext_mem_erase_mode'):
                cli_opts += f",ext_mem_erase_mode={opts['ext_mem_erase_mode']}"
            if opts.get('verify'):
                cli_opts += f",verify={opts['verify']}"
            cmd.append(cli_opts)
        elif op_type == 'reset':
            cmd += ['--reset-kind', _op['kind']]
        elif op_type == 'erase':
            cmd.append(f'--{_op["kind"]}')
        elif op_type == 'x-provision-keys':
            cmd += ['--key-file', _op['keyfile']]

        cmd += ['--core', op['core']] if op.get('core') else []
        cmd += ['--x-family', f'{self.family}']
        cmd += ['--x-append-batch', f'{json_file}']
        self._exec(cmd)

    def _exec_batch(self):
        # Use x-append-batch to get the JSON from nrfutil itself
        json_file = Path(self.hex_).parent / 'generated_nrfutil_batch.json'
        json_file.unlink(missing_ok=True)
        for op in self._ops:
            self._append_batch(op, json_file)

        # reset first in case an exception is thrown
        self._ops = []
        self._op_id = 1
        self.logger.debug(f'Executing batch in: {json_file}')
        precmd = []
        if self.ext_mem_config_file:
            # This needs to be prepended, as it's a global option
            precmd = ['--x-ext-mem-config-file', self.ext_mem_config_file]

        self._exec(precmd + ['x-execute-batch', '--batch-path', f'{json_file}',
                             '--serial-number', self._format_dev_ids()])

    def do_exec_op(self, op, force=False):
        self.logger.debug(f'Executing op: {op}')
        if force:
            if len(self._ops) != 0:
                raise RuntimeError(f'Forced exec with {len(self._ops)} ops')
            self._insert_op(op)
            self._exec_batch()
            return True
        # Defer by default
        return False

    def flush_ops(self, force=True):
        if not force:
            return
        while self.ops:
            self._insert_op(self.ops.popleft())
        self._exec_batch()
