#!/usr/bin/env python3
#
# Copyright (c) 2024 Nordic Semiconductor NA
#
# SPDX-License-Identifier: Apache-2.0

from jinja2 import Environment, FileSystemLoader, select_autoescape

env = Environment(
    loader=FileSystemLoader("."),
    autoescape=select_autoescape()
)

dppic_inst = [
    "0",
    "00",
    "10",
    "20",
    "30",
    "020",
    "030",
    "120",
    "130",
    "131",
    "132",
    "133",
    "134",
    "135",
    "136"
]
ppib_inst = [
    ("00", "10"),
    ("01", "20"),
    ("11", "21"),
    ("22", "30"),
    ("02", "03"),
    ("04", "12"),
    ("020", "030")
]

template = env.get_template("nrfx_config_reserved_resources.h.jinja2")
print(template.render(ppib_inst=ppib_inst, dppic_inst=dppic_inst))
