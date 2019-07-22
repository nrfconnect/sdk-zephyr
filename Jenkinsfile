
@Library("CI_LIB") _

def AGENT_LABELS = lib_Main.getAgentLabels(JOB_NAME)
def IMAGE_TAG    = lib_Main.getDockerImage(JOB_NAME)
def TIMEOUT      = lib_Main.getTimeout(JOB_NAME)
def INPUT_STATE  = lib_Main.getInputState(JOB_NAME)
def CI_STATE = new HashMap()

pipeline {

  parameters {
   booleanParam(name: 'RUN_DOWNSTREAM', description: 'if false skip downstream jobs', defaultValue: true)
   booleanParam(name: 'RUN_TESTS', description: 'if false skip testing', defaultValue: true)
   booleanParam(name: 'RUN_BUILD', description: 'if false skip building', defaultValue: true)
   string(name: 'PLATFORMS', description: 'Default Platforms to test', defaultValue: 'nrf9160_pca10090 nrf52_pca10040 nrf52840_pca10056')
   string(name: 'jsonstr_CI_STATE', description: 'Default State if no upstream job',
              defaultValue: INPUT_STATE)
  }

  agent {
    docker {
      image IMAGE_TAG
      label AGENT_LABELS
      args '-e PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/workdir/.local/bin'
    }
  }
  options {
    // Checkout the repository to this folder instead of root
    checkoutToSubdirectory('zephyr')
    timeout(time: TIMEOUT.time, unit: TIMEOUT.unit)
  }

  environment {
      // ENVs for check-compliance
      GH_TOKEN = credentials('nordicbuilder-compliance-token') // This token is used to by check_compliance to comment on PRs and use checks
      GH_USERNAME = "NordicBuilder"
      COMPLIANCE_ARGS = "-r NordicPlayground/fw-nrfconnect-zephyr --exclude-module Kconfig"

      LC_ALL = "C.UTF-8"

      // ENVs for sanitycheck
      ARCH = "-a arm"
      SANITYCHECK_OPTIONS = "--inline-logs --enable-coverage -N"
      SANITYCHECK_RETRY = "--only-failed --outdir=out-2nd-pass"
      SANITYCHECK_RETRY_2 = "--only-failed --outdir=out-3rd-pass"

      // ENVs for building (triggered by sanitycheck)
      ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
      GNUARMEMB_TOOLCHAIN_PATH = '/workdir/gcc-arm-none-eabi-7-2018-q2-update'
      ZEPHYR_SDK_INSTALL_DIR = '/opt/zephyr-sdk'
  }

  stages {
    stage('Load') { steps { script { CI_STATE = lib_Stage.load('ZEPHYR') }}}
    stage('Checkout') {
      steps { script {
        lib_Main.cloneCItools(JOB_NAME)
        dir('zephyr') {
          CI_STATE.ZEPHYR.REPORT_SHA = lib_Main.checkoutRepo(CI_STATE.ZEPHYR.GIT_URL, "ZEPHYR", CI_STATE.ZEPHYR, false)
          lib_West.AddManifestUpdate("ZEPHYR", 'zephyr', CI_STATE.ZEPHYR.GIT_URL, CI_STATE.ZEPHYR.GIT_REF, CI_STATE)
        }
      }}
    }
    stage('Get nRF && Apply Parent Manifest Updates') {
      when { expression { CI_STATE.ZEPHYR.RUN_TESTS || CI_STATE.ZEPHYR.RUN_BUILD } }
      steps { script {
        lib_Status.set("PENDING", 'ZEPHYR', CI_STATE);
        lib_West.InitUpdate('zephyr')
        lib_West.ApplyManifestUpdates(CI_STATE)
      }}
    }
    stage('Run compliance check') {
      when { expression { CI_STATE.ZEPHYR.RUN_TESTS } }
      steps {
        dir('zephyr') {
          script {
            def BUILD_TYPE = lib_Main.getBuildType(CI_STATE.ZEPHYR)
            if (BUILD_TYPE == "PR") {
              COMMIT_RANGE = "$CI_STATE.ZEPHYR.MERGE_BASE..$CI_STATE.ZEPHYR.REPORT_SHA"
              COMPLIANCE_ARGS = "$COMPLIANCE_ARGS -p $CHANGE_ID -S $CI_STATE.ZEPHYR.REPORT_SHA -g"
              println "Building a PR [$CHANGE_ID]: $COMMIT_RANGE"
            }
            else if (BUILD_TYPE == "TAG") {
              COMMIT_RANGE = "tags/${env.BRANCH_NAME}..tags/${env.BRANCH_NAME}"
              println "Building a Tag: " + COMMIT_RANGE
            }
            // If not a PR, it's a non-PR-branch or master build. Compare against the origin.
            else if (BUILD_TYPE == "BRANCH") {
              COMMIT_RANGE = "origin/${env.BRANCH_NAME}..HEAD"
              println "Building a Branch: " + COMMIT_RANGE
            }
            else {
                assert condition : "Build fails because it is not a PR/Tag/Branch"
            }

            // Run the compliance check
            try {
              sh "(source ../zephyr/zephyr-env.sh && ../ci-tools/scripts/check_compliance.py $COMPLIANCE_ARGS --commits $COMMIT_RANGE)"
            }
            finally {
              junit 'compliance.xml'
              archiveArtifacts artifacts: 'compliance.xml'
            }
          }
        }
      }
    }
    stage('Sanitycheck (nRF)') {
      when { expression { CI_STATE.ZEPHYR.RUN_BUILD  && (CI_STATE.ORIGIN.BUILD_TYPE == 'PR') } }
      steps { script {
        dir('zephyr') {
          sh "ls -alh "
          sh "echo variant: $ZEPHYR_TOOLCHAIN_VARIANT"
          sh "echo SDK dir: $ZEPHYR_SDK_INSTALL_DIR"
          sh "cat /opt/zephyr-sdk/sdk_version"
          def PLATFORM_ARGS = lib_Main.getPlatformArgs(CI_STATE.ZEPHYR.PLATFORMS)
          sh "source zephyr-env.sh && \
              (./scripts/sanitycheck $SANITYCHECK_OPTIONS $ARCH $PLATFORM_ARGS || \
              (sleep 10; ./scripts/sanitycheck $SANITYCHECK_OPTIONS $SANITYCHECK_RETRY) || \
              (sleep 10; ./scripts/sanitycheck $SANITYCHECK_OPTIONS $SANITYCHECK_RETRY_2))"
        }
      }}
    }
    stage('Sanitycheck (all)') {
      when { expression { CI_STATE.ZEPHYR.RUN_BUILD  && (CI_STATE.ORIGIN.BUILD_TYPE != 'PR') } }
      steps { script {
        dir('zephyr') {
          sh "ls -alh "
          sh "echo variant: $ZEPHYR_TOOLCHAIN_VARIANT"
          sh "echo SDK dir: $ZEPHYR_SDK_INSTALL_DIR"
          sh "cat /opt/zephyr-sdk/sdk_version"
          sh "source zephyr-env.sh && \
              (./scripts/sanitycheck $SANITYCHECK_OPTIONS $ARCH || \
              (sleep 10; ./scripts/sanitycheck $SANITYCHECK_OPTIONS $SANITYCHECK_RETRY) || \
              (sleep 10; ./scripts/sanitycheck $SANITYCHECK_OPTIONS $SANITYCHECK_RETRY_2))"
        }
      } }
    }

    stage('Trigger testing build') {
      when { expression { CI_STATE.ZEPHYR.RUN_DOWNSTREAM } }
      steps {
        script {
          CI_STATE.ZEPHYR.WAITING = true
          def DOWNSTREAM_JOBS = lib_Main.getDownStreamJobs(JOB_NAME)
          println "DOWNSTREAM_JOBS = " + DOWNSTREAM_JOBS

          def jobs = [:]
          DOWNSTREAM_JOBS.each {
            jobs["${it}"] = {
              build job: "${it}", propagate: true, wait: CI_STATE.ZEPHYR.WAITING, parameters: [
                        string(name: 'jsonstr_CI_STATE', value: lib_Util.HashMap2Str(CI_STATE))]
            }
          }
          parallel jobs
        }
      }
    }
  }

  post {
    // This is the order that the methods are run. {always->success/abort/failure/unstable->cleanup}
    always {
      echo "always"
    }
    success {
      echo "success"
      script { lib_Status.set("SUCCESS", 'ZEPHYR', CI_STATE) }
    }
    aborted {
      echo "aborted"
      script { lib_Status.set("ABORTED", 'ZEPHYR', CI_STATE) }
    }
    unstable {
      echo "unstable"
    }
    failure {
      echo "failure"
      script { lib_Status.set("FAILURE", 'ZEPHYR', CI_STATE) }
    }
    cleanup {
        echo "cleanup"
        cleanWs()
    }
  }
}
