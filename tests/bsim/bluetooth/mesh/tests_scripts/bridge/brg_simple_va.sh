#!/usr/bin/env bash
# Copyright 2024 Nordic Semiconductor
# SPDX-License-Identifier: Apache-2.0

source $(dirname "${BASH_SOURCE[0]}")/../../_mesh_test.sh

# This test checks basic functionality of the Subnet Bridge with virtual addressing. It checks the
# following:
# - Messages are bridged to virtual address subscribers, only for subnets in the bridging table.
# - Messages are not bridged when the Subnet Bridge state is disabled.
#
# 3 roles are used in this test: Tester, Subnet Bridge node, and Mesh node.
#
# Subnets topology*:
#                  Tester
#                    |
#                (subnet 0)
#                   |
#              Subnet Bridge (bridges subnets 1 and 2)
#                   |
#              Virtual Address
#            /      |       \
#    (subnet 1)  (subnet 2)  (subnet 3)**
#       |          |             \
#     Node       Node           Node
#
# (*)  - All nodes are in the tester's range
# (**) - Messages are not bridged to subnet 3 via the virtual address. If the node belonging to
#        subnet 3 receives a message from the tester, the test will fail.
#
# Test procedure:
# The same procedure as in the `mesh_brg_simple` test is used. The main differences are:
# - An additional node is added to a new subnet (3).
# - Each of the nodes are subscribed to the same virtual address. Messages are bridged from the
#   tester to the virtual address, only for subnets 1 and 2.
# - To allow nodes to respond to the tester, messages from each node is bridged to the tester.

RunTest mesh_brg_simple_va \
	brg_tester_simple_va brg_bridge_simple brg_device_simple brg_device_simple brg_device_simple

overlay=overlay_psa_conf
RunTest mesh_brg_simple_va_psa \
	brg_tester_simple_va brg_bridge_simple brg_device_simple brg_device_simple brg_device_simple
