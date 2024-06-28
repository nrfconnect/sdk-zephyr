import logging

from twister_harness import DeviceAdapter

logger = logging.getLogger(__name__)

def test_flash(dut: DeviceAdapter):
    found = False
    output = dut.readlines()
    for line in output:
        if 'Hello' in line:
            found = True
            logger.debug(f'Match in {line}')
    assert found
