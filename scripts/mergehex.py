#!/usr/bin/env python3
# vim: set syntax=python ts=4 :
"""Zephyr merge hex util
"""

from intelhex import IntelHex
import argparse


def merge_hex_files(output, *):



def parse_arguments():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.fromfile_prefix_chars = "+"

    parser.add_argument(
        "-i", "--input", action="append",
        help="Input files to merge.")
    parser.add_argument(
        "-o", "--output", action="append",
        help="Output file.")
    return parser.parse_args()


def main():
    global options
    options = parse_arguments()

if __name__ == "__main__":
    main()
