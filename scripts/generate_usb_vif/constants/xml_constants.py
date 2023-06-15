#!/usr/bin/env python3

# Copyright (c) 2022 The Chromium OS Authors
# SPDX-License-Identifier: Apache-2.0

"""This file contains XML constants defined to be used by generate_vif.py"""

from constants import other_constants
from constants import vif_element_constants

XML_ENCODING = "utf-8"
XML_ELEMENT_NAME_PREFIX = "vif"
XML_ROOT_ELEMENT_NAME = "VIF"
XML_VIF_NAMESPACE = "http://usb.org/VendorInfoFile.xsd"
XML_NAMESPACE_ATTRIBUTES = {
    "xmlns:vif": XML_VIF_NAMESPACE,
}

VIF_SPEC_ELEMENTS = {
    "VIF_Specification": {
        other_constants.TEXT: "3.19",
    },
    "VIF_App": {
        other_constants.CHILD: {
            "Description": {
                other_constants.TEXT: "This VIF XML file is generated by the Zephyr GenVIF script",
            }
        }
    },
}

VIF_SPEC_ELEMENTS_FROM_SOURCE_XML = {vif_element_constants.VENDOR_NAME,
                                     vif_element_constants.MODEL_PART_NUMBER,
                                     vif_element_constants.PRODUCT_REVISION,
                                     vif_element_constants.TID,
                                     vif_element_constants.VIF_PRODUCT_TYPE,
                                     vif_element_constants.CERTIFICATION_TYPE, }
