def IMAGE_TAG = "ncs-toolchain:1.07"
def REPO_CI_TOOLS = "https://github.com/zephyrproject-rtos/ci-tools.git"
def REPO_NRFXLIB = "https://github.com/NordicPlayground/nrfxlib.git"
def REPO_NRF = "https://github.com/NordicPlayground/fw-nrfconnect-nrf.git"

pipeline {
  agent {
    docker {
      image "$IMAGE_TAG"
      label "docker && ncs"
    }
  }
  options {
    // Checkout the repository to this folder instead of root
    checkoutToSubdirectory('zephyr')
  }

  environment {
      // ENVs for check-compliance
      GH_TOKEN = credentials('nordicbuilder-compliance-token') // This token is used to by check_compliance to comment on PRs and use checks
      GH_USERNAME = "NordicBuilder"
      COMPLIANCE_ARGS = "-g -r NordicPlayground/fw-nrfconnect-zephyr -p $CHANGE_ID -S $GIT_COMMIT"
      LC_ALL = "C.UTF-8"

      // ENVs for sanitycheck
      ARCH = "-a arm"
      SANITYCHECK_OPTIONS_ALL = "--inline-logs --enable-coverage -N"
      SANITYCHECK_OPTIONS_NRF = "--board-root $WORKSPACE/nrf/boards --testcase-root $WORKSPACE/nrf/samples --build-only --disable-unrecognized-section-test -t ci_build --inline-logs --enable-coverage -N"

      // ENVs for building (triggered by sanitycheck)
      ZEPHYR_TOOLCHAIN_VARIANT = 'gnuarmemb'
      GNUARMEMB_TOOLCHAIN_PATH = '/workdir/gcc-arm-none-eabi-7-2018-q2-update'
  }

  stages {
    stage('Checkout repositories') {
      steps {
        dir("ci-tools") {
          git branch: "master", url: "$REPO_CI_TOOLS"
        }
        dir("nrfxlib") {
          git branch: "master", url: "$REPO_NRFXLIB", credentialsId: 'github'
        }
        dir("nrf") {
          git branch: "master", url: "$REPO_NRF", credentialsId: 'github'
        }
      }
    }

    stage('Testing') {
      parallel {
        stage('Run compliance check') {
          steps {
            dir('zephyr') {
              script {
                // If we're a pull request, compare the target branch against the current HEAD (the PR)
                if (env.CHANGE_TARGET) {
                  COMMIT_RANGE = "origin/${env.CHANGE_TARGET}..HEAD"
                }
                // If not a PR, it's a non-PR-branch or master build. Compare against the origin.
                else {
                  COMMIT_RANGE = "origin/${env.BRANCH_NAME}..HEAD"
                }
                // Run the compliance check
                try {
                  sh "source zephyr-env.sh && ../ci-tools/scripts/check_compliance.py $COMPLIANCE_ARGS --commits $COMMIT_RANGE"
                }
                finally {
                  junit 'compliance.xml'
                  archiveArtifacts artifacts: 'compliance.xml'
                }
              }
            }
          }
        }

        stage('Sanitycheck (all)') {
          steps {
            dir('zephyr') {
              sh "source zephyr-env.sh && ./scripts/sanitycheck $SANITYCHECK_OPTIONS_ALL $ARCH"
            }
          }
        }

        stage('Build nrf samples') {
          steps {
            dir('zephyr') {
              sh "source zephyr-env.sh && ./scripts/sanitycheck $SANITYCHECK_OPTIONS_NRF"
            }
          }
        }
      }
    }
  }

  post {
    always {
      // Clean up the working space at the end (including tracked files)
      cleanWs()
    }
  }
}
