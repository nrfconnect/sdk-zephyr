# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import logging
import re
import time

from twister_harness.device.device_adapter import DeviceAdapter
from twister_harness.exceptions import TwisterHarnessTimeoutException

logger = logging.getLogger(__name__)


class Shell:
    """
    Helper class that provides methods used to interact with shell application.
    """

    def __init__(self, device: DeviceAdapter, prompt: str = 'uart:~$', timeout: float | None = None) -> None:
        self._device: DeviceAdapter = device
        self.prompt: str = prompt
        self.base_timeout: float = timeout or device.base_timeout

    def wait_for_prompt(self, timeout: float | None = None) -> bool:
        """
        Send every 0.5 second "enter" command to the device until shell prompt
        statement will occur (return True) or timeout will be exceeded (return
        False).
        """
        timeout = timeout or self.base_timeout
        timeout_time = time.time() + timeout
        self._device.clear_buffer()
        while time.time() < timeout_time:
            self._device.write(b'\n')
            try:
                line = self._device.readline(timeout=0.5, print_output=False)
            except TwisterHarnessTimeoutException:
                # ignore read timeout and try to send enter once again
                continue
            if self.prompt in line:
                logger.debug('Got prompt')
                time.sleep(0.05)
                self._device.clear_buffer()
                return True
        return False

    def exec_command(self, command: str, timeout: float | None = None, print_output: bool = True) -> list[str]:
        """
        Send shell command to a device and return response. Passed command
        is extended by double enter sings - first one to execute this command
        on a device, second one to receive next prompt what is a signal that
        execution was finished.
        """
        timeout = timeout or self.base_timeout
        command_ext = f'{command}\n\n'
        regex_prompt = re.escape(self.prompt)
        regex_command = f'.*{command}'
        self._device.clear_buffer()
        self._device.write(command_ext.encode())
        lines: list[str] = []
        # wait for device command print - it should be done immediately after sending command to device
        lines.extend(self._device.readlines_until(regex=regex_command, timeout=1.0, print_output=print_output))
        # wait for device command execution
        lines.extend(self._device.readlines_until(regex=regex_prompt, timeout=timeout, print_output=print_output))
        return lines
