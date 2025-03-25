#!/usr/bin/env python3
# Copyright (c) 2025, Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import signal
import subprocess
import sys
from importlib.metadata import PackageNotFoundError, version

from packaging.version import parse as parse_version


def signal_handler(sig, frame):
    print('\nScript terminated by user')
    sys.exit(0)


def check_requirements():
    try:
        installed_version = version('nrfcloud-utils')
        min_required_version = "1.0.4"
        if parse_version(installed_version) < parse_version(min_required_version):
            print(
                f"\033[31mError: device_credentials_installer >= {min_required_version} required. "
                f"Installed: {installed_version}.\033[0m"
            )
            print("Update: pip3 install --upgrade nrfcloud-utils")
            sys.exit(1)
    except PackageNotFoundError:
        print("\033[31mError: device_credentials_installer could not be found.\033[0m")
        print("Please install it using: pip3 install nrfcloud-utils")
        sys.exit(1)


def main():
    signal.signal(signal.SIGINT, signal_handler)
    parser = argparse.ArgumentParser(description='Install Wi-Fi certificates', allow_abbrev=False)
    parser.add_argument('--path', required=True, help='Path to certificate files')
    parser.add_argument(
        '--serial-device', default='/dev/ttyACM1', help='Serial port device (default: /dev/ttyACM1)'
    )
    parser.add_argument(
        '--operation-mode',
        choices=['AP', 'STA'],
        default='STA',
        help='Operation mode: AP or STA (default: STA)',
    )
    args = parser.parse_args()

    cert_path = args.path
    cert_path = args.path
    port = args.serial_device
    mode = args.operation_mode
    if not os.path.isdir(cert_path):
        print(f"\033[31mError: Directory {cert_path} does not exist.\033[0m")
        sys.exit(1)

    print(
        "\033[33mWarning: Please make sure that the UART is not being used by another "
        "application.\033[0m"
    )
    input("Press Enter to continue or Ctrl+C to exit...")

    check_requirements()

    # TLS credential types
    TLS_CREDENTIAL_CA_CERTIFICATE = 0
    TLS_CREDENTIAL_PUBLIC_CERTIFICATE = 1
    TLS_CREDENTIAL_PRIVATE_KEY = 2

    WIFI_CERT_SEC_TAG_BASE = 0x1020001
    WIFI_CERT_SEC_TAG_MAP = {
        "ca.pem": (TLS_CREDENTIAL_CA_CERTIFICATE, WIFI_CERT_SEC_TAG_BASE),
        "client-key.pem": (TLS_CREDENTIAL_PRIVATE_KEY, WIFI_CERT_SEC_TAG_BASE + 1),
        "server-key.pem": (TLS_CREDENTIAL_PRIVATE_KEY, WIFI_CERT_SEC_TAG_BASE + 2),
        "client.pem": (TLS_CREDENTIAL_PUBLIC_CERTIFICATE, WIFI_CERT_SEC_TAG_BASE + 3),
        "server.pem": (TLS_CREDENTIAL_PUBLIC_CERTIFICATE, WIFI_CERT_SEC_TAG_BASE + 4),
        "ca2.pem": (TLS_CREDENTIAL_CA_CERTIFICATE, WIFI_CERT_SEC_TAG_BASE + 5),
        "client-key2.pem": (TLS_CREDENTIAL_PRIVATE_KEY, WIFI_CERT_SEC_TAG_BASE + 6),
        "client2.pem": (TLS_CREDENTIAL_PUBLIC_CERTIFICATE, WIFI_CERT_SEC_TAG_BASE + 7),
    }

    cert_files = (
        ["ca.pem", "server-key.pem", "server.pem"]
        if mode == "AP"
        else ["ca.pem", "client-key.pem", "client.pem", "ca2.pem", "client-key2.pem", "client2.pem"]
    )

    total_certs = len(cert_files)
    for idx, cert in enumerate(cert_files, 1):
        print(f"Processing certificate {idx} of {total_certs}: {cert}")

        cert_file_path = os.path.join(cert_path, cert)
        if not os.path.isfile(cert_file_path):
            print(
                f"\033[31mWarning: Certificate file {cert_file_path} does not exist. "
                f"Skipping...\033[0m"
            )
            continue

        cert_type, sec_tag = WIFI_CERT_SEC_TAG_MAP[cert]
        try:
            subprocess.run(
                [
                    "device_credentials_installer",
                    "--local-cert-file",
                    cert_file_path,
                    "--cmd-type",
                    "tls_cred_shell",
                    "--delete",
                    "--port",
                    port,
                    "-S",
                    str(sec_tag),
                    "--cert-type",
                    str(cert_type),
                ],
                check=True,
            )
            print(f"Successfully installed {cert}.")
        except subprocess.CalledProcessError:
            print(f"\033[31mFailed to install {cert}.\033[0m")

    print("Certificate installation process completed.")


if __name__ == "__main__":
    main()
