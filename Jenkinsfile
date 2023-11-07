@Library("CI_LIB") _

def pipeline_zephyr = new ncs.sdk_zephyr.Main()

pipeline_zephyr.run(JOB_NAME)

def pipeline_nrf = new ncs.sdk_nrf.Main()

pipeline:nrf.run(JOB_NAME)
