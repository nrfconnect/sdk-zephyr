#!/usr/bin/env python3
#
# Copyright (c) 2017 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

import sys
import argparse
import pprint
import os
import struct
from elf_helper import ElfHelper

kobjects = [
    "k_alert",
    "k_msgq",
    "k_mutex",
    "k_pipe",
    "k_sem",
    "k_stack",
    "k_thread",
    "k_timer",
    "_k_thread_stack_element",
    "device"
]

subsystems = [
    "adc_driver_api",
    "aio_cmp_driver_api",
    "counter_driver_api",
    "crypto_driver_api",
    "dma_driver_api",
    "flash_driver_api",
    "gpio_driver_api",
    "i2c_driver_api",
    "i2s_driver_api",
    "ipm_driver_api",
    "pinmux_driver_api",
    "pwm_driver_api",
    "entropy_driver_api",
    "rtc_driver_api",
    "sensor_driver_api",
    "spi_driver_api",
    "uart_driver_api",
]


header = """%compare-lengths
%define lookup-function-name _k_object_lookup
%language=ANSI-C
%global-table
%struct-type
%{
#include <kernel.h>
#include <syscall_handler.h>
#include <string.h>
%}
struct _k_object;
%%
"""


# Different versions of gperf have different prototypes for the lookup
# function, best to implement the wrapper here. The pointer value itself is
# turned into a string, we told gperf to expect binary strings that are not
# NULL-terminated.
footer = """%%
struct _k_object *_k_object_find(void *obj)
{
    return _k_object_lookup((const char *)obj, sizeof(void *));
}

void _k_object_wordlist_foreach(_wordlist_cb_func_t func, void *context)
{
    int i;

    for (i = MIN_HASH_VALUE; i <= MAX_HASH_VALUE; i++) {
        if (wordlist[i].name) {
            func(&wordlist[i], context);
        }
    }
}
"""


def write_gperf_table(fp, eh, objs, static_begin, static_end):
    fp.write(header)

    for obj_addr, ko in objs.items():
        obj_type = ko.type_name
        # pre-initialized objects fall within this memory range, they are
        # either completely initialized at build time, or done automatically
        # at boot during some PRE_KERNEL_* phase
        initialized = obj_addr >= static_begin and obj_addr < static_end

        byte_str = struct.pack("<I" if eh.little_endian else ">I", obj_addr)
        fp.write("\"")
        for byte in byte_str:
            val = "\\x%02x" % byte
            fp.write(val)

        fp.write(
            "\",{},%s,%s,%d\n" %
            (obj_type,
             "K_OBJ_FLAG_INITIALIZED" if initialized else "0",
             ko.data))

    fp.write(footer)


def parse_args():
    global args

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument("-k", "--kernel", required=True,
                        help="Input zephyr ELF binary")
    parser.add_argument(
        "-o", "--output", required=True,
        help="Output list of kernel object addresses for gperf use")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print extra debugging information")
    args = parser.parse_args()
    if "VERBOSE" in os.environ:
        args.verbose = 1


def main():
    parse_args()

    eh = ElfHelper(args.kernel, args.verbose, kobjects, subsystems)
    syms = eh.get_symbols()
    max_threads = syms["CONFIG_MAX_THREAD_BYTES"] * 8
    objs = eh.find_kobjects(syms)

    if eh.get_thread_counter() > max_threads:
        sys.stderr.write("Too many thread objects (%d)\n" % thread_counter)
        sys.stderr.write("Increase CONFIG_MAX_THREAD_BYTES to %d\n",
                         -(-thread_counter // 8))
        sys.exit(1)

    with open(args.output, "w") as fp:
        write_gperf_table(fp, eh, objs, syms["_static_kernel_objects_begin"],
                          syms["_static_kernel_objects_end"])


if __name__ == "__main__":
    main()
