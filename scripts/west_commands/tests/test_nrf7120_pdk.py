# Copyright (c) 2026 Nordic Semiconductor ASA.
#
# SPDX-License-Identifier: Apache-2.0

import os
from types import SimpleNamespace

import pytest
from intelhex import IntelHex

from runners.nrf7120_pdk import (
    CTRL_AP,
    CTRL_AP_BOOTSTATUS,
    CTRL_AP_ERASEALL,
    CTRL_AP_ERASEALLSTATUS,
    DHCSR,
    DHCSR_RESUME,
    LCS_TEST,
    MRAMC_CONFIGNVR0,
    MRAMC_WEN,
    SCB_VTOR,
    Nrf7120PdkBinaryRunner,
)

SNR = 1052802863


class FakeJLink:
    def __init__(self, serials=(SNR,), lcs=LCS_TEST):
        self.serials = serials
        self.is_open = False
        self.select = 0
        self.memory = {}
        self.memory_writes = []
        self.ap_registers = {
            (CTRL_AP, CTRL_AP_BOOTSTATUS): lcs << 12,
            (CTRL_AP, CTRL_AP_ERASEALLSTATUS): 0,
        }

    def connected_emulators(self):
        return [SimpleNamespace(SerialNumber=serial) for serial in self.serials]

    def open(self, serial_no=None):
        assert serial_no in self.serials
        self.is_open = True

    def opened(self):
        return self.is_open

    def close(self):
        self.is_open = False

    def set_tif(self, interface):
        pass

    def coresight_configure(self):
        pass

    def exec_command(self, command):
        pass

    def connect(self, device, speed, verbose):
        assert device == 'CORTEX-M33'

    def halt(self):
        return True

    def coresight_write(self, reg, data, ap):
        if not ap:
            assert reg == 2
            self.select = data
            return
        ap_index = self.select >> 24
        address = (self.select & 0xF0) | (reg * 4)
        self.ap_registers[(ap_index, address)] = data
        if (ap_index, address) == (CTRL_AP, CTRL_AP_ERASEALL):
            self.ap_registers[(CTRL_AP, CTRL_AP_ERASEALLSTATUS)] = 0

    def coresight_read(self, reg, ap):
        assert ap
        ap_index = self.select >> 24
        address = (self.select & 0xF0) | (reg * 4)
        return self.ap_registers.get((ap_index, address), 0)

    def memory_read8(self, address, count):
        return [self.memory.get(address + offset, 0xff)
                for offset in range(count)]

    def memory_write8(self, address, values):
        self.memory_writes.append((address, list(values), 8))
        for offset, value in enumerate(values):
            self.memory[address + offset] = value

    def memory_write32(self, address, values):
        self.memory_writes.append((address, list(values), 32))
        for index, value in enumerate(values):
            for offset, byte in enumerate(value.to_bytes(4, 'little')):
                self.memory[address + index * 4 + offset] = byte

    def memory_write(self, address, values, nbits):
        assert nbits == 32
        self.memory_write32(address, values)


def image_config(runner_config, tmp_path, name, address, data,
                 load_offset=0):
    build_dir = tmp_path / name
    zephyr = build_dir / 'zephyr'
    zephyr.mkdir(parents=True)
    (zephyr / '.config').write_text(f'''
CONFIG_SOC_NRF7120_ENGA_CPUAPP=y
CONFIG_FLASH_BASE_ADDRESS=0x0
CONFIG_FLASH_LOAD_OFFSET={load_offset:#x}
''')
    hex_file = zephyr / 'zephyr.hex'
    image = IntelHex()
    image.puts(address, data)
    image.write_hex_file(hex_file)
    return runner_config._replace(build_dir=os.fspath(build_dir),
                                  hex_file=os.fspath(hex_file))


def test_nrf7120_pdk_programs_verifies_and_starts(runner_config, tmp_path,
                                                   monkeypatch):
    vector = (0x20001948).to_bytes(4, 'little')
    vector += (0x0000b395).to_bytes(4, 'little')
    vector += b'\x11\x22\x33\x44'
    cfg = image_config(runner_config, tmp_path, 'app', 0xa000, vector,
                       load_offset=0xa000)
    fake = FakeJLink()
    monkeypatch.setattr(
        'runners.nrf7120_pdk.pylink.JLink', lambda: fake)

    runner = Nrf7120PdkBinaryRunner(cfg, dev_id=None, reset=True)
    runner.do_run('flash')

    assert runner.dev_id == str(SNR)
    assert bytes(fake.memory[0xa000 + index]
                 for index in range(len(vector))) == vector
    assert fake.memory[MRAMC_WEN] == 0
    assert fake.memory[MRAMC_CONFIGNVR0] == 0
    assert (SCB_VTOR, [0xa000], 32) in fake.memory_writes
    assert (DHCSR, [DHCSR_RESUME], 32) in fake.memory_writes
    assert not fake.opened()


def test_nrf7120_pdk_rejects_non_test_lcs(runner_config, tmp_path,
                                           monkeypatch):
    vector = (0x20001948).to_bytes(4, 'little')
    vector += (0x0000b395).to_bytes(4, 'little')
    cfg = image_config(runner_config, tmp_path, 'app', 0xa000, vector,
                       load_offset=0xa000)
    fake = FakeJLink(lcs=0x99CC9)
    monkeypatch.setattr(
        'runners.nrf7120_pdk.pylink.JLink', lambda: fake)

    runner = Nrf7120PdkBinaryRunner(cfg, dev_id=SNR, reset=True)
    with pytest.raises(RuntimeError, match='already be in TEST LCS'):
        runner.do_run('flash')
    assert not fake.opened()


def test_nrf7120_pdk_requires_unique_probe(runner_config, tmp_path,
                                           monkeypatch):
    vector = (0x20001948).to_bytes(4, 'little')
    vector += (0x0000b395).to_bytes(4, 'little')
    cfg = image_config(runner_config, tmp_path, 'app', 0xa000, vector,
                       load_offset=0xa000)
    fake = FakeJLink(serials=(SNR, 123456789))
    monkeypatch.setattr(
        'runners.nrf7120_pdk.pylink.JLink', lambda: fake)

    runner = Nrf7120PdkBinaryRunner(cfg, dev_id=None, reset=True)
    with pytest.raises(RuntimeError, match='multiple J-Link probes'):
        runner.do_run('flash')


def test_nrf7120_pdk_collects_sysbuild_images(runner_config, tmp_path):
    vector = (0x20001948).to_bytes(4, 'little')
    vector += (0x0000b395).to_bytes(4, 'little')
    app_cfg = image_config(runner_config, tmp_path, 'app', 0xa000, vector,
                           load_offset=0xa000)
    uicr_cfg = image_config(runner_config, tmp_path, 'uicr',
                            0x00ffd000, b'\x01\x02\x03\x04')

    app = Nrf7120PdkBinaryRunner(app_cfg, dev_id=SNR, reset=False)
    app.do_run('flash')
    args = SimpleNamespace(dev_id=None)
    Nrf7120PdkBinaryRunner.args_from_previous_runner(app, args)

    final = Nrf7120PdkBinaryRunner(
        uicr_cfg, dev_id=args.dev_id, reset=False,
        hex_files=args.nrf7120_pdk_hex_files,
        startup=args.nrf7120_pdk_startup)
    final.do_run('flash')

    assert final.hex_files == [app_cfg.hex_file, uicr_cfg.hex_file]
    assert final.startup == (0xa000, 0x20001948, 0x0000b394)
