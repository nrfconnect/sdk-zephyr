
@Library("CI_LIB") _

def AGENT_LABELS = lib_Main.getAgentLabels(JOB_NAME)
def IMAGE_TAG    = lib_Main.getDockerImage(JOB_NAME)
def TIMEOUT      = lib_Main.getTimeout(JOB_NAME)
def INPUT_STATE  = lib_Main.getInputState(JOB_NAME)
def CI_STATE = new HashMap()
def TestExecutionList = [:]

pipeline {

  parameters {
   booleanParam(name: 'RUN_DOWNSTREAM', description: 'if false skip downstream jobs', defaultValue: false)
   booleanParam(name: 'RUN_TESTS', description: 'if false skip testing', defaultValue: true)
   booleanParam(name: 'RUN_BUILD', description: 'if false skip building', defaultValue: true)
   booleanParam(name: 'RUN_BUILD_UPSTREAM', description: 'if false skip building', defaultValue: true)
   string(name: 'SANITYCHECK_RETRY_NUM', description: 'Default number of sanitycheck retries', defaultValue: '7')
   string(name: 'PLATFORMS', description: 'Default Platforms to test', defaultValue: 'nrf9160_pca10090 nrf9160_pca10090ns nrf52_pca10040 nrf52840_pca10056')
   string(name: 'jsonstr_CI_STATE', description: 'Default State if no upstream job',
              defaultValue: INPUT_STATE)
  }
  agent { label AGENT_LABELS }

  options {
    // Checkout the repository to this folder instead of root
    checkoutToSubdirectory('zephyr')
    parallelsAlwaysFailFast()
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
      SANITYCHECK_OPTIONS = "--inline-logs --enable-coverage -N" // DEFAULT: --testcase-root tests
      SANITYCHECK_RETRY_CMDS = '' // initialized so that it is shared to parrallel stages
  }

  stages {
    stage('Load') { steps { script { CI_STATE = lib_Stage.load('ZEPHYR') }}}
    stage('Specification') { steps { script {

      def TestStages = [:]

      TestStages["compliance"] = {
        node (AGENT_LABELS) {
          stage('Compliance Test'){
            println "Using Node:$NODE_NAME"
            docker.image("$DOCKER_REG/$IMAGE_TAG").inside {
              dir('zephyr') {
                checkout scm
                CI_STATE.ZEPHYR.REPORT_SHA = lib_Main.checkoutRepo(
                      CI_STATE.ZEPHYR.GIT_URL, "ZEPHYR", CI_STATE.ZEPHYR, false)
                lib_Status.set("PENDING", 'ZEPHYR', CI_STATE);
                lib_West.AddManifestUpdate("ZEPHYR", 'zephyr',
                      CI_STATE.ZEPHYR.GIT_URL, CI_STATE.ZEPHYR.GIT_REF, CI_STATE)
              }
              lib_West.InitUpdate('zephyr')
              lib_West.ApplyManifestUpdates(CI_STATE)

              dir('zephyr') {
                def BUILD_TYPE = lib_Main.getBuildType(CI_STATE.ZEPHYR)
                if (BUILD_TYPE == "PR") {
                  if (CI_STATE.ZEPHYR.IS_MERGEUP) {
                    println 'This is a MERGE-UP PR.   CI_STATE.ZEPHYR.IS_MERGEUP=' + CI_STATE.ZEPHYR.IS_MERGEUP
                    CI_STATE.ZEPHYR.MERGEUP_BASE = sh( script: "git log --oneline --grep='\\[nrf mergeup\\].*' -i -n 1 --pretty=format:'%h' | tr -d '\\n'" , returnStdout: true)
                    println "CI_STATE.ZEPHYR.MERGEUP_BASE = $CI_STATE.ZEPHYR.MERGEUP_BASE"
                    COMMIT_RANGE = "$CI_STATE.ZEPHYR.MERGEUP_BASE..$CI_STATE.ZEPHYR.REPORT_SHA"
                  } else {
                    COMMIT_RANGE = "$CI_STATE.ZEPHYR.MERGE_BASE..$CI_STATE.ZEPHYR.REPORT_SHA"
                  }
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
                  sh "(source ../zephyr/zephyr-env.sh && ../tools/ci-tools/scripts/check_compliance.py $COMPLIANCE_ARGS --commits $COMMIT_RANGE)"
                }
                finally {
                  junit 'compliance.xml'
                  archiveArtifacts artifacts: 'compliance.xml'
                }
              }
            }
          }
        }
      }

      CI_STATE.ZEPHYR.IS_MERGEUP = false
      if (((CI_STATE.ZEPHYR.CHANGE_TITLE.toLowerCase().contains('mergeup')  ) || (CI_STATE.ZEPHYR.CHANGE_TITLE.toLowerCase().contains('upmerge')  )) &&
          ((CI_STATE.ZEPHYR.CHANGE_BRANCH.toLowerCase().contains('mergeup') ) || (CI_STATE.ZEPHYR.CHANGE_BRANCH.toLowerCase().contains('upmerge') ))) {
        CI_STATE.ZEPHYR.IS_MERGEUP = true
        println 'This is a MERGE-UP PR.   CI_STATE.ZEPHYR.IS_MERGEUP=' + CI_STATE.ZEPHYR.IS_MERGEUP
      }

      if (CI_STATE.ZEPHYR.RUN_TESTS) {
        TestExecutionList['compliance'] = TestStages["compliance"]
      }

      if (CI_STATE.ZEPHYR.RUN_BUILD) {
        def SUBSET_LIST = ['1/4', '2/4', '3/4', '4/4' ]
        def COMPILER_LIST = ['gnuarmemb'] // 'zephyr',
        def INPUT_MAP = [set : SUBSET_LIST, compiler : COMPILER_LIST ]

        def OUTPUT_MAP = INPUT_MAP.values().combinations { args ->
            [INPUT_MAP.keySet().toList(), args].transpose().collectEntries { [(it[0]): it[1]]}
        }

        SANITYCHECK_RETRY_CMDS_LIST = []
        for (i=1; i <= SANITYCHECK_RETRY_NUM.toInteger(); i++) {
          SANITYCHECK_RETRY_CMDS_LIST.add("(sleep 30; ./scripts/sanitycheck $SANITYCHECK_OPTIONS --only-failed)")
        }
        SANITYCHECK_RETRY_CMDS = SANITYCHECK_RETRY_CMDS_LIST.join(' || \n')

        def sanityCheckNRFStages = OUTPUT_MAP.collectEntries {
            ["SanityCheckNRF\n${it.compiler}\n${it.set}" : generateParallelStageNRF(it.set, it.compiler,
                  AGENT_LABELS, DOCKER_REG, IMAGE_TAG, JOB_NAME, CI_STATE)]
        }
        TestExecutionList = TestExecutionList.plus(sanityCheckNRFStages)

        if (CI_STATE.ZEPHYR.RUN_BUILD_UPSTREAM) {
          def sanityCheckALLStages = OUTPUT_MAP.collectEntries {
              ["SanityCheckALL\nzephyr\n${it.set}" : generateParallelStageALL(it.set, 'zephyr',
                    AGENT_LABELS, DOCKER_REG, IMAGE_TAG, JOB_NAME, CI_STATE)]
          }
          TestExecutionList = TestExecutionList.plus(sanityCheckALLStages)
        }
      }

      println "TestExecutionList = $TestExecutionList"

    }}}

    stage('Execution') { steps { script {
      parallel TestExecutionList
    }}}

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
      script { lib_Status.set("FAILURE", 'ZEPHYR', CI_STATE) }
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

def generateParallelStageNRF(subset, compiler, AGENT_LABELS, DOCKER_REG, IMAGE_TAG, JOB_NAME, CI_STATE) {
  return {
    node (AGENT_LABELS) {
      stage('Sanity Check - Zephyr'){
        println "Using Node:$NODE_NAME"
        docker.image("$DOCKER_REG/$IMAGE_TAG").inside {
          dir('zephyr') {
            checkout scm
            CI_STATE.ZEPHYR.REPORT_SHA = lib_Main.checkoutRepo(
                  CI_STATE.ZEPHYR.GIT_URL, "ZEPHYR", CI_STATE.ZEPHYR, false)
            lib_West.AddManifestUpdate("ZEPHYR", 'zephyr',
                  CI_STATE.ZEPHYR.GIT_URL, CI_STATE.ZEPHYR.GIT_REF, CI_STATE)
          }
          lib_West.InitUpdate('zephyr')
          lib_West.ApplyManifestUpdates(CI_STATE)
          dir('zephyr') {
            def PLATFORM_ARGS = lib_Main.getPlatformArgs(CI_STATE.ZEPHYR.PLATFORMS)
            println "$compiler SANITY NRF PLATFORMS_ARGS = $PLATFORM_ARGS"
            sh """
                export ZEPHYR_TOOLCHAIN_VARIANT='$compiler' && \
                source zephyr-env.sh && \
                ./scripts/sanitycheck $SANITYCHECK_OPTIONS $ARCH $PLATFORM_ARGS --subset $subset || $SANITYCHECK_RETRY_CMDS
               """
          }
        }
        cleanWs()
      }
    }
  }
}

def generateParallelStageALL(subset, compiler, AGENT_LABELS, DOCKER_REG, IMAGE_TAG, JOB_NAME, CI_STATE) {
  return {
    node (AGENT_LABELS) {
      stage('Sanity Check - Zephyr'){
        println "Using Node:$NODE_NAME"
        docker.image("$DOCKER_REG/$IMAGE_TAG").inside {
          dir('zephyr') {
            checkout scm
            CI_STATE.ZEPHYR.REPORT_SHA = lib_Main.checkoutRepo(
                  CI_STATE.ZEPHYR.GIT_URL, "ZEPHYR", CI_STATE.ZEPHYR, false)
            lib_West.AddManifestUpdate("ZEPHYR", 'zephyr',
                  CI_STATE.ZEPHYR.GIT_URL, CI_STATE.ZEPHYR.GIT_REF, CI_STATE)
          }
          lib_West.InitUpdate('zephyr')
          lib_West.ApplyManifestUpdates(CI_STATE)
          dir('zephyr') {
            sh """
                export ZEPHYR_TOOLCHAIN_VARIANT='$compiler' && \
                source zephyr-env.sh && \
                ./scripts/sanitycheck $SANITYCHECK_OPTIONS $ARCH --subset $subset || $SANITYCHECK_RETRY_CMDS
               """
          }
        }
      }
    }
  }
}
