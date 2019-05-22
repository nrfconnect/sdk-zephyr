# SPDX-License-Identifier: Apache-2.0
import re
from collections import OrderedDict

result_re = re.compile("(PASS|FAIL|SKIP) - (test_)?(.*)")

class Harness:
    GCOV_START = "GCOV_COVERAGE_DUMP_START"
    GCOV_END = "GCOV_COVERAGE_DUMP_END"
    FAULTS = [
            "Unknown Fatal Error",
            "MPU FAULT",
            "Kernel Panic",
            "Kernel OOPS",
            "BUS FAULT",
            "CPU Page Fault"
            ]

    def __init__(self):
        self.state = None
        self.type = None
        self.regex = []
        self.matches = OrderedDict()
        self.ordered = True
        self.repeat = 1
        self.tests = {}
        self.id = None
        self.fail_on_fault = True
        self.fault = False
        self.capture_coverage = False

    def configure(self, instance):
        config = instance.test.harness_config
        self.id = instance.test.id
        if "ignore_faults" in instance.test.tags:
            self.fail_on_fault = False

        if config:
            self.type = config.get('type', None)
            self.regex = config.get('regex', [] )
            self.repeat = config.get('repeat', 1)
            self.ordered = config.get('ordered', True)

class Console(Harness):

    def configure(self, instance):
        super(Console, self).configure(instance)
        if self.type == "one_line":
            self.pattern = re.compile(self.regex[0])
        elif self.type == "multi_line":
            self.patterns = []
            for r in self.regex:
                self.patterns.append(re.compile(r))

    def handle(self, line):

        if self.type == "one_line":
            if self.pattern.search(line):
                self.state = "passed"
        elif self.type == "multi_line":
            for i, pattern in enumerate(self.patterns):
                r = self.regex[i]
                if pattern.search(line) and not r in self.matches:
                    self.matches[r] = line

            if len(self.matches) == len(self.regex):
                # check ordering
                if self.ordered:
                    ordered = True
                    pos = 0
                    for k in self.matches:
                        if k != self.regex[pos]:
                            ordered = False
                        pos += 1

                    if ordered:
                        self.state = "passed"
                    else:
                        self.state = "failed"
                else:
                    self.state = "passed"

        if self.fail_on_fault:
            for fault in self.FAULTS:
                if fault in line:
                    self.fault = True

        if self.GCOV_START in line:
            self.capture_coverage = True
        elif self.GCOV_END in line:
            self.capture_coverage = False

        if self.state == "passed":
            self.tests[self.id] = "PASS"
        else:
            self.tests[self.id] = "FAIL"


class Test(Harness):
    RUN_PASSED = "PROJECT EXECUTION SUCCESSFUL"
    RUN_FAILED = "PROJECT EXECUTION FAILED"

    def handle(self, line):
        match = result_re.match(line)
        if match:
            name = "{}.{}".format(self.id, match.group(3))
            self.tests[name] = match.group(1)

        if self.RUN_PASSED in line:
            if self.fault:
                self.state = "failed"
            else:
                self.state = "passed"

        if self.RUN_FAILED in line:
            self.state = "failed"

        if self.fail_on_fault:
            for fault in self.FAULTS:
                if fault in line:
                    self.fault = True

        if self.GCOV_START in line:
            self.capture_coverage = True
        elif self.GCOV_END in line:
            self.capture_coverage = False
