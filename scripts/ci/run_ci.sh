#!/bin/bash
# Copyright (c) 2017 Linaro Limited
# Copyright (c) 2018 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0
#
# Author: Anas Nashif
#
# This script is run in CI and support both pull request and commit modes. So
# it can be used also on commits to the tree or on tags.
# The following options are supports:
# -p  start the script for pull requests
# -m  matrix node number, for example 3 on a 5 node matrix
# -M  total number of nodes in the matrix
# -b  base branch
# -r  the remote to rebase on
#
# The script can be run locally using for example:
# ./scripts/ci/run_ci.sh -b master -r origin  -l -R <commit range>

set -xe

sanitycheck_options=" --inline-logs -N --timestamps -v"
export BSIM_OUT_PATH="${BSIM_OUT_PATH:-/opt/bsim/}"
if [ ! -d "${BSIM_OUT_PATH}" ]; then
        unset BSIM_OUT_PATH
fi
export BSIM_COMPONENTS_PATH="${BSIM_OUT_PATH}/components/"
bsim_bt_test_results_file="./bsim_bt_out/bsim_results.xml"
west_commands_results_file="./pytest_out/west_commands.xml"

matrix_builds=1
matrix=1

function handle_coverage() {
	# this is for shippable coverage reports
	echo "Calling gcovr"
	gcovr -r ${ZEPHYR_BASE} -x > shippable/codecoverage/coverage.xml


	# Upload to codecov.io only on merged builds or if CODECOV_IO variable
	# is set.
	if [ -n "${CODECOV_IO}" -o -z "${pull_request_nr}" ]; then
		# Capture data
		echo "Running lcov --capture ..."
		lcov --capture \
			--directory sanity-out/native_posix/ \
			--directory sanity-out/nrf52_bsim/ \
			--directory sanity-out/unit_testing/ \
			--directory bsim_bt_out/ \
			--output-file lcov.pre.info -q --rc lcov_branch_coverage=1

		# Remove noise
		echo "Exclude data from coverage report..."
		lcov -q \
			--remove lcov.pre.info mylib.c \
			--remove lcov.pre.info tests/\* \
			--remove lcov.pre.info samples/\* \
			--remove lcov.pre.info ext/\* \
			--remove lcov.pre.info *generated* \
			-o lcov.info --rc lcov_branch_coverage=1

		# Cleanup
		rm lcov.pre.info
		rm -rf sanity-out out-2nd-pass

		# Upload to codecov.io
		echo "Upload coverage reports to codecov.io"
		bash <(curl -s https://codecov.io/bash) -f "lcov.info" -X coveragepy -X fixes
		rm -f lcov.info
	fi

	rm -rf sanity-out out-2nd-pass

}

function handle_compiler_cache() {
	# Give more details in case we fail because of compiler cache
	if [ -f "$HOME/.cache/zephyr/ToolchainCapabilityDatabase.cmake" ]; then
		echo "Dumping the capability database in case we are affected by #9992"
		cat $HOME/.cache/zephyr/ToolchainCapabilityDatabase.cmake
	fi
}

function on_complete() {
	source zephyr-env.sh
	if [ "$1" == "failure" ]; then
		handle_compiler_cache
	fi

	rm -rf ccache $HOME/.cache/zephyr
	mkdir -p shippable/testresults
	mkdir -p shippable/codecoverage

	if [ -e ./sanity-out/sanitycheck.xml ]; then
		echo "Copy ./sanity-out/sanitycheck.xml"
		cp ./sanity-out/sanitycheck.xml shippable/testresults/
	fi

	if [ -e ./module_tests/sanitycheck.xml ]; then
		echo "Copy ./module_tests/sanitycheck.xml"
		cp ./module_tests/sanitycheck.xml \
			shippable/testresults/module_tests.xml
	fi

	if [ -e ${bsim_bt_test_results_file} ]; then
		echo "Copy ${bsim_bt_test_results_file}"
		cp ${bsim_bt_test_results_file} shippable/testresults/
	fi

	if [ -e ${west_commands_results_file} ]; then
		echo "Copy ${west_commands_results_file}"
		cp ${west_commands_results_file} shippable/testresults
	fi

	if [ "$matrix" = "1" ]; then
		echo "Skip handling coverage data..."
		#handle_coverage
	else
		rm -rf sanity-out out-2nd-pass
	fi
}

function run_bsim_bt_tests() {
	WORK_DIR=${ZEPHYR_BASE}/bsim_bt_out tests/bluetooth/bsim_bt/compile.sh
	RESULTS_FILE=${ZEPHYR_BASE}/${bsim_bt_test_results_file} \
	SEARCH_PATH=tests/bluetooth/bsim_bt/bsim_test_app/tests_scripts \
	tests/bluetooth/bsim_bt/run_parallel.sh
}

function get_tests_to_run() {
	./scripts/zephyr_module.py --sanitycheck-out module_tests.args
	./scripts/ci/get_modified_tests.py --commits ${commit_range} > modified_tests.args
	./scripts/ci/get_modified_boards.py --commits ${commit_range} > modified_boards.args

	if [ -s modified_boards.args ]; then
		${sanitycheck} ${sanitycheck_options} +modified_boards.args --save-tests test_file_1.txt || exit 1
	fi
	if [ -s modified_tests.args ]; then
		${sanitycheck} ${sanitycheck_options} +modified_tests.args --save-tests test_file_2.txt || exit 1
	fi
	rm -f modified_tests.args modified_boards.args
}


function west_setup() {
	# West handling
	git_dir=$(basename $PWD)
	pushd ..
	if [ ! -d .west ]; then
		west init -l ${git_dir}
		west update
	fi
	popd
}


while getopts ":p:m:b:r:M:cfslR:" opt; do
	case $opt in
		c)
			echo "Execute CI" >&2
			main_ci=1
			;;
		l)
			echo "Executing script locally" >&2
			local_run=1
			main_ci=1
			;;
		s)
			echo "Success" >&2
			success=1
			;;
		f)
			echo "Failure" >&2
			failure=1
			;;
		p)
			echo "Testing a Pull Request: $OPTARG." >&2
			pull_request_nr=$OPTARG
			;;
		m)
			echo "Running on Matrix $OPTARG" >&2
			matrix=$OPTARG
			;;
		M)
			echo "Running a matrix of $OPTARG slaves" >&2
			matrix_builds=$OPTARG
			;;
		b)
			echo "Base Branch: $OPTARG" >&2
			branch=$OPTARG
			;;
		r)
			echo "Remote: $OPTARG" >&2
			remote=$OPTARG
			;;
		R)
			echo "Range: $OPTARG" >&2
			range=$OPTARG
			;;
		\?)
			echo "Invalid option: -$OPTARG" >&2
			;;
	esac
done

if [ -n "$main_ci" ]; then

	west_setup

	if [ -z "$branch" ]; then
		echo "No base branch given"
		exit 1
	else
		commit_range=$remote/${branch}..HEAD
		echo "Commit range:" ${commit_range}
	fi
	if [ -n "$range" ]; then
		commit_range=$range
	fi
	source zephyr-env.sh
	sanitycheck="${ZEPHYR_BASE}/scripts/sanitycheck"

	# Possibly the only record of what exact version is being tested:
	short_git_log='git log -n 5 --oneline --decorate --abbrev=12 '

	if [ -n "$pull_request_nr" ]; then
		$short_git_log $remote/${branch}
		# Now let's pray this script is being run from a
		# different location
# https://stackoverflow.com/questions/3398258/edit-shell-script-while-its-running
		git rebase $remote/${branch}
	fi
	$short_git_log

	if [ -n "${BSIM_OUT_PATH}" -a -d "${BSIM_OUT_PATH}" ]; then
		echo "Build and run BT simulator tests"
		# Run BLE tests in simulator on the 1st CI instance:
		if [ "$matrix" = "1" ]; then
			run_bsim_bt_tests
		fi
	else
		echo "Skipping BT simulator tests"
	fi

	if [ "$matrix" = "1" ]; then
		# Run pytest-based testing for Python in matrix
		# builder 1.  For now, this is just done for the west
		# extension commands, but additional directories which
		# run pytest could go here too.
		pytest=$(type -p pytest-3 || echo "pytest")
		mkdir -p $(dirname ${west_commands_results_file})
		PYTHONPATH=./scripts/west_commands "${pytest}" \
			  --junitxml=${west_commands_results_file} \
			  ./scripts/west_commands/tests
	else
		echo "Skipping west command tests"
	fi

	# cleanup
	rm -f test_file.txt
	touch test_file_1.txt test_file_2.txt

	# In a pull-request see if we have changed any tests or board definitions
	if [ -n "${pull_request_nr}" -o -n "${local_run}"  ]; then
		get_tests_to_run
	fi

	# Save list of tests to be run
	${sanitycheck} ${sanitycheck_options} --save-tests test_file_3.txt || exit 1
	cat test_file_1.txt test_file_2.txt test_file_3.txt > test_file.txt

	# Run a subset of tests based on matrix size
	${sanitycheck} ${sanitycheck_options} --load-tests test_file.txt \
		--subset ${matrix}/${matrix_builds} --retry-failed 3

	# Run module tests on matrix #1
	if [ "$matrix" = "1" ]; then
		if [ -s module_tests.args ]; then
			${sanitycheck} ${sanitycheck_options} \
				+module_tests.args --outdir module_tests
		fi
	fi

	# cleanup
	rm -f test_file*

elif [ -n "$failure" ]; then
	on_complete failure
elif [ -n "$success" ]; then
	on_complete
else
	echo "Nothing to do"
fi

