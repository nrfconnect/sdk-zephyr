"""
Copyright (c) 2025 Nordic Semiconductor ASA
SPDX-License-Identifier: Apache-2.0
"""

from __future__ import annotations

import argparse
import ctypes as c
import math
import sys
from itertools import groupby

from elftools.elf.elffile import ELFFile
from intelhex import IntelHex

# The UICR format version produced by this script
UICR_FORMAT_VERSION_MAJOR = 2
UICR_FORMAT_VERSION_MINOR = 0

# Name of the ELF section containing PERIPHCONF entries.
# Must match the name used in the linker script.
PERIPHCONF_SECTION = "uicr_periphconf_entry"

# Common values for representing enabled/disabled in the UICR format.
ENABLED_VALUE = 0xFFFF_FFFF
DISABLED_VALUE = 0xBD23_28A8


class ScriptError(RuntimeError): ...


class PeriphconfEntry(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("regptr", c.c_uint32),
        ("value", c.c_uint32),
    ]


PERIPHCONF_ENTRY_SIZE = c.sizeof(PeriphconfEntry)


class Version(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("MINOR", c.c_uint16),
        ("MAJOR", c.c_uint16),
    ]


class Approtect(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("APPLICATION", c.c_uint32),
        ("RADIOCORE", c.c_uint32),
        ("RESERVED", c.c_uint32),
        ("CORESIGHT", c.c_uint32),
    ]


class Protectedmem(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("ENABLE", c.c_uint32),
        ("SIZE4KB", c.c_uint32),
    ]


class Recovery(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("ENABLE", c.c_uint32),
        ("PROCESSOR", c.c_uint32),
        ("INITSVTOR", c.c_uint32),
        ("SIZE4KB", c.c_uint32),
    ]


class Its(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("ENABLE", c.c_uint32),
        ("ADDRESS", c.c_uint32),
        ("APPLICATIONSIZE", c.c_uint32),
        ("RADIOCORESIZE", c.c_uint32),
    ]


class Periphconf(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("ENABLE", c.c_uint32),
        ("ADDRESS", c.c_uint32),
        ("MAXCOUNT", c.c_uint32),
    ]


class Mpcconf(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("ENABLE", c.c_uint32),
        ("ADDRESS", c.c_uint32),
        ("MAXCOUNT", c.c_uint32),
    ]


class Uicr(c.LittleEndianStructure):
    _pack_ = 1
    _fields_ = [
        ("VERSION", Version),
        ("RESERVED", c.c_uint32),
        ("LOCK", c.c_uint32),
        ("RESERVED1", c.c_uint32),
        ("APPROTECT", Approtect),
        ("ERASEPROTECT", c.c_uint32),
        ("PROTECTEDMEM", Protectedmem),
        ("RECOVERY", Recovery),
        ("ITS", Its),
        ("RESERVED2", c.c_uint32 * 7),
        ("PERIPHCONF", Periphconf),
        ("MPCCONF", Mpcconf),
    ]


def main() -> None:
    parser = argparse.ArgumentParser(
        allow_abbrev=False,
        description=(
            "Generate artifacts for the UICR and associated configuration blobs from application "
            "build outputs. User Information Configuration Registers (UICR), in the context of "
            "certain Nordic SoCs, are used to configure system resources, like memory and "
            "peripherals, and to protect the device in various ways."
        ),
    )
    parser.add_argument(
        "--in-periphconf-elf",
        dest="in_periphconf_elfs",
        default=[],
        action="append",
        type=argparse.FileType("rb"),
        help=(
            "Path to an ELF file to extract PERIPHCONF data from. Can be provided multiple times. "
            "The PERIPHCONF data from each ELF file is combined in a single list which is sorted "
            "by ascending address and cleared of duplicate entries."
        ),
    )
    parser.add_argument(
        "--out-uicr-hex",
        required=True,
        type=argparse.FileType("w", encoding="utf-8"),
        help="Path to write the generated merged UICR+PERIPHCONF HEX file to (typically zephyr.hex)",
    )
    parser.add_argument(
        "--periphconf-address",
        default=None,
        type=lambda s: int(s, 0),
        help="Absolute flash address of the PERIPHCONF partition (decimal or 0x-prefixed hex)",
    )
    parser.add_argument(
        "--periphconf-size",
        default=None,
        type=lambda s: int(s, 0),
        help="Size in bytes of the PERIPHCONF partition (decimal or 0x-prefixed hex)",
    )
    parser.add_argument(
        "--uicr-address",
        required=True,
        type=lambda s: int(s, 0),
        help="Absolute flash address of the UICR region (decimal or 0x-prefixed hex)",
    )
    args = parser.parse_args()

    try:
        init_values = DISABLED_VALUE.to_bytes(4, "little") * (c.sizeof(Uicr) // 4)
        uicr = Uicr.from_buffer_copy(init_values)

        uicr.VERSION.MAJOR = UICR_FORMAT_VERSION_MAJOR
        uicr.VERSION.MINOR = UICR_FORMAT_VERSION_MINOR

        # Create a single hex object that will contain both UICR and periphconf data
        merged_hex = IntelHex()

        if args.in_periphconf_elfs:  # Check if periphconf data is provided
            periphconf_combined = extract_and_combine_periphconfs(args.in_periphconf_elfs)

            padding_len = args.periphconf_size - len(periphconf_combined)
            periphconf_final = periphconf_combined + bytes([0xFF for _ in range(padding_len)])

            # Add periphconf data to the merged hex
            merged_hex.frombytes(periphconf_final, offset=args.periphconf_address)

            uicr.PERIPHCONF.ENABLE = ENABLED_VALUE
            uicr.PERIPHCONF.ADDRESS = args.periphconf_address
            uicr.PERIPHCONF.MAXCOUNT = math.floor(args.periphconf_size / 8)

        # Add UICR data to the merged hex
        merged_hex.frombytes(bytes(uicr), offset=args.uicr_address)

        # Write the merged hex file containing both UICR and periphconf data
        merged_hex.write_hex_file(args.out_uicr_hex)

    except ScriptError as e:
        print(f"Error: {e!s}")
        sys.exit(1)


def extract_and_combine_periphconfs(elf_files: list[argparse.FileType]) -> bytes:
    combined_periphconf = []

    for in_file in elf_files:
        elf = ELFFile(in_file)
        conf_section = elf.get_section_by_name(PERIPHCONF_SECTION)
        if conf_section is None:
            continue

        conf_section_data = conf_section.data()
        num_entries = len(conf_section_data) // PERIPHCONF_ENTRY_SIZE
        periphconf = (PeriphconfEntry * num_entries).from_buffer_copy(conf_section_data)
        combined_periphconf.extend(periphconf)

    combined_periphconf.sort(key=lambda e: e.regptr)
    deduplicated_periphconf = []

    for regptr, regptr_entries in groupby(combined_periphconf, key=lambda e: e.regptr):
        entries = list(regptr_entries)
        if len(entries) > 1:
            unique_values = {e.value for e in entries}
            if len(unique_values) > 1:
                raise ScriptError(
                    f"PERIPHCONF has conflicting values for register 0x{regptr:09_x}: "
                    + ", ".join([f"0x{val:09_x}" for val in unique_values])
                )
        deduplicated_periphconf.append(entries[0])

    final_periphconf = (PeriphconfEntry * len(deduplicated_periphconf))()
    for i, entry in enumerate(deduplicated_periphconf):
        final_periphconf[i] = entry

    return bytes(final_periphconf)


if __name__ == "__main__":
    main()
