#!/usr/bin/env python3
# vim: set syntax=python ts=4 :
#
# Copyright (c) 2018 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import os
import json
import logging
from colorama import Fore
import xml.etree.ElementTree as ET
import string
from datetime import datetime

logger = logging.getLogger('twister')
logger.setLevel(logging.DEBUG)

class Reporting:

    def __init__(self, plan, env) -> None:
        self.plan = plan #FIXME
        self.instances = plan.instances
        self.platforms = plan.platforms
        self.selected_platforms = plan.selected_platforms
        self.filtered_platforms = plan.filtered_platforms
        self.env = env
        self.timestamp = datetime.now().isoformat()
        self.outdir = os.path.abspath(env.options.outdir)

    @staticmethod
    def process_log(log_file):
        filtered_string = ""
        if os.path.exists(log_file):
            with open(log_file, "rb") as f:
                log = f.read().decode("utf-8")
                filtered_string = ''.join(filter(lambda x: x in string.printable, log))

        return filtered_string


    @staticmethod
    def xunit_testcase(eleTestsuite, name, classname, status, ts_status, reason, duration, runnable, stats, log, build_only_as_skip):
        fails, passes, errors, skips = stats

        if status in ['skipped', 'filtered']:
            duration = 0

        eleTestcase = ET.SubElement(
            eleTestsuite, "testcase",
            classname=classname,
            name=f"{name}",
            time=f"{duration}")

        if status in ['skipped', 'filtered']:
            skips += 1
            # temporarily add build_only_as_skip to restore existing CI report behaviour
            if ts_status == "passed" and not runnable:
                tc_type = "build"
            else:
                tc_type = status
            ET.SubElement(eleTestcase, 'skipped', type=f"{tc_type}", message=f"{reason}")
        elif status in ["failed", "blocked"]:
            fails += 1
            el = ET.SubElement(eleTestcase, 'failure', type="failure", message=f"{reason}")
            if log:
                el.text = log
        elif status == "error":
            errors += 1
            el = ET.SubElement(eleTestcase, 'error', type="failure", message=f"{reason}")
            if log:
                el.text = log
        elif status == 'passed':
            if not runnable and build_only_as_skip:
                ET.SubElement(eleTestcase, 'skipped', type="build", message="built only")
                skips += 1
            else:
                passes += 1
        else:
            if not status:
                logger.debug(f"{name}: No status")
                ET.SubElement(eleTestcase, 'skipped', type=f"untested", message="No results captured, testsuite misconfiguration?")
            else:
                logger.error(f"{name}: Unknown status '{status}'")

        return (fails, passes, errors, skips)

    # Generate a report with all testsuites instead of doing this per platform
    def xunit_report_suites(self, json_file, filename):

        json_data = {}
        with open(json_file, "r") as json_results:
            json_data = json.load(json_results)


        env = json_data.get('environment', {})
        version = env.get('zephyr_version', None)

        eleTestsuites = ET.Element('testsuites')
        all_suites = json_data.get("testsuites", [])

        suites_to_report = all_suites
            # do not create entry if everything is filtered out
        if not self.env.options.detailed_skipped_report:
            suites_to_report = list(filter(lambda d: d.get('status') != "filtered", all_suites))

        for suite in suites_to_report:
            duration = 0
            eleTestsuite = ET.SubElement(eleTestsuites, 'testsuite',
                                            name=suite.get("name"), time="0",
                                            timestamp = self.timestamp,
                                            tests="0",
                                            failures="0",
                                            errors="0", skipped="0")
            eleTSPropetries = ET.SubElement(eleTestsuite, 'properties')
            # Multiple 'property' can be added to 'properties'
            # differing by name and value
            ET.SubElement(eleTSPropetries, 'property', name="version", value=version)
            ET.SubElement(eleTSPropetries, 'property', name="platform", value=suite.get("platform"))
            ET.SubElement(eleTSPropetries, 'property', name="architecture", value=suite.get("arch"))

            total = 0
            fails = passes = errors = skips = 0
            handler_time = suite.get('execution_time', 0)
            runnable = suite.get('runnable', 0)
            duration += float(handler_time)
            ts_status = suite.get('status')
            for tc in suite.get("testcases", []):
                status = tc.get('status')
                reason = tc.get('reason', suite.get('reason', 'Unknown'))
                log = tc.get("log", suite.get("log"))

                tc_duration = tc.get('execution_time', handler_time)
                name = tc.get("identifier")
                classname = ".".join(name.split(".")[:2])
                fails, passes, errors, skips = self.xunit_testcase(eleTestsuite,
                    name, classname, status, ts_status, reason, tc_duration, runnable,
                    (fails, passes, errors, skips), log, True)

            total = (errors + passes + fails + skips)

            eleTestsuite.attrib['time'] = f"{duration}"
            eleTestsuite.attrib['failures'] = f"{fails}"
            eleTestsuite.attrib['errors'] = f"{errors}"
            eleTestsuite.attrib['skipped'] = f"{skips}"
            eleTestsuite.attrib['tests'] = f"{total}"

        result = ET.tostring(eleTestsuites)
        with open(filename, 'wb') as report:
            report.write(result)

    def xunit_report(self, json_file, filename, selected_platform=None, full_report=False):
        if selected_platform:
            selected = [selected_platform]
            logger.info(f"Writing target report for {selected_platform}...")
        else:
            logger.info(f"Writing xunit report {filename}...")
            selected = self.selected_platforms

        json_data = {}
        with open(json_file, "r") as json_results:
            json_data = json.load(json_results)


        env = json_data.get('environment', {})
        version = env.get('zephyr_version', None)

        eleTestsuites = ET.Element('testsuites')
        all_suites = json_data.get("testsuites", [])

        for platform in selected:
            suites = list(filter(lambda d: d['platform'] == platform, all_suites))
            # do not create entry if everything is filtered out
            if not self.env.options.detailed_skipped_report:
                non_filtered = list(filter(lambda d: d.get('status') != "filtered", suites))
                if not non_filtered:
                    continue

            duration = 0
            eleTestsuite = ET.SubElement(eleTestsuites, 'testsuite',
                                            name=platform,
                                            timestamp = self.timestamp,
                                            time="0",
                                            tests="0",
                                            failures="0",
                                            errors="0", skipped="0")
            eleTSPropetries = ET.SubElement(eleTestsuite, 'properties')
            # Multiple 'property' can be added to 'properties'
            # differing by name and value
            ET.SubElement(eleTSPropetries, 'property', name="version", value=version)

            total = 0
            fails = passes = errors = skips = 0
            for ts in suites:
                handler_time = ts.get('execution_time', 0)
                runnable = ts.get('runnable', 0)
                duration += float(handler_time)

                ts_status = ts.get('status')
                # Do not report filtered testcases
                if ts_status == 'filtered' and not self.env.options.detailed_skipped_report:
                    continue
                if full_report:
                    for tc in ts.get("testcases", []):
                        status = tc.get('status')
                        reason = tc.get('reason', ts.get('reason', 'Unknown'))
                        log = tc.get("log", ts.get("log"))

                        tc_duration = tc.get('execution_time', handler_time)
                        name = tc.get("identifier")
                        classname = ".".join(name.split(".")[:2])
                        fails, passes, errors, skips = self.xunit_testcase(eleTestsuite,
                            name, classname, status, ts_status, reason, tc_duration, runnable,
                            (fails, passes, errors, skips), log, True)
                else:
                    reason = ts.get('reason', 'Unknown')
                    name = ts.get("name")
                    classname = f"{platform}:{name}"
                    log = ts.get("log")
                    fails, passes, errors, skips = self.xunit_testcase(eleTestsuite,
                        name, classname, ts_status, ts_status, reason, duration, runnable,
                        (fails, passes, errors, skips), log, False)

            total = (errors + passes + fails + skips)

            eleTestsuite.attrib['time'] = f"{duration}"
            eleTestsuite.attrib['failures'] = f"{fails}"
            eleTestsuite.attrib['errors'] = f"{errors}"
            eleTestsuite.attrib['skipped'] = f"{skips}"
            eleTestsuite.attrib['tests'] = f"{total}"

        result = ET.tostring(eleTestsuites)
        with open(filename, 'wb') as report:
            report.write(result)

    def json_report(self, filename, version="NA"):
        logger.info(f"Writing JSON report {filename}")
        report = {}
        report["environment"] = {"os": os.name,
                                 "zephyr_version": version,
                                 "toolchain": self.env.toolchain,
                                 "commit_date": self.env.commit_date,
                                 "run_date": self.env.run_date
                                 }
        suites = []

        for instance in self.instances.values():
            suite = {}
            handler_log = os.path.join(instance.build_dir, "handler.log")
            build_log = os.path.join(instance.build_dir, "build.log")
            device_log = os.path.join(instance.build_dir, "device.log")

            handler_time = instance.metrics.get('handler_time', 0)
            used_ram = instance.metrics.get ("used_ram", 0)
            used_rom  = instance.metrics.get("used_rom",0)
            available_ram = instance.metrics.get("available_ram", 0)
            available_rom = instance.metrics.get("available_rom", 0)
            suite = {
                "name": instance.testsuite.name,
                "arch": instance.platform.arch,
                "platform": instance.platform.name,
            }
            if instance.run_id:
                suite['run_id'] = instance.run_id

            suite["runnable"] = False
            if instance.status != 'filtered':
                suite["runnable"] = instance.run

            if used_ram:
                suite["used_ram"] = used_ram
            if used_rom:
                suite["used_rom"] = used_rom

            suite['retries'] = instance.retries

            if available_ram:
                suite["available_ram"] = available_ram
            if available_rom:
                suite["available_rom"] = available_rom
            if instance.status in ["error", "failed"]:
                suite['status'] = instance.status
                suite["reason"] = instance.reason
                # FIXME
                if os.path.exists(handler_log):
                    suite["log"] = self.process_log(handler_log)
                elif os.path.exists(device_log):
                    suite["log"] = self.process_log(device_log)
                else:
                    suite["log"] = self.process_log(build_log)
            elif instance.status == 'filtered':
                suite["status"] = "filtered"
                suite["reason"] = instance.reason
            elif instance.status == 'passed':
                suite["status"] = "passed"
            elif instance.status == 'skipped':
                suite["status"] = "skipped"
                suite["reason"] = instance.reason

            if instance.status is not None:
                suite["execution_time"] =  f"{float(handler_time):.2f}"

            testcases = []

            if len(instance.testcases) == 1:
                single_case_duration = f"{float(handler_time):.2f}"
            else:
                single_case_duration = 0

            for case in instance.testcases:
                # freeform was set when no sub testcases were parsed, however,
                # if we discover those at runtime, the fallback testcase wont be
                # needed anymore and can be removed from the output, it does
                # not have a status and would otherwise be reported as skipped.
                if case.freeform and case.status is None and len(instance.testcases) > 1:
                    continue
                testcase = {}
                testcase['identifier'] = case.name
                if instance.status:
                    if single_case_duration:
                        testcase['execution_time'] = single_case_duration
                    else:
                        testcase['execution_time'] = f"{float(case.duration):.2f}"

                if case.output != "":
                    testcase['log'] = case.output

                if case.status == "skipped":
                    if instance.status == "filtered":
                        testcase["status"] = "filtered"
                    else:
                        testcase["status"] = "skipped"
                        testcase["reason"] = case.reason or instance.reason
                else:
                    testcase["status"] = case.status
                    if case.reason:
                        testcase["reason"] = case.reason

                testcases.append(testcase)

            suite['testcases'] = testcases
            suites.append(suite)

        report["testsuites"] = suites
        with open(filename, "wt") as json_file:
            json.dump(report, json_file, indent=4, separators=(',',':'))


    def compare_metrics(self, filename):
        # name, datatype, lower results better
        interesting_metrics = [("used_ram", int, True),
                               ("used_rom", int, True)]

        if not os.path.exists(filename):
            logger.error("Cannot compare metrics, %s not found" % filename)
            return []

        results = []
        saved_metrics = {}
        with open(filename) as fp:
            jt = json.load(fp)
            for ts in jt.get("testsuites", []):
                d = {}
                for m, _, _ in interesting_metrics:
                    d[m] = ts.get(m, 0)
                ts_name = ts.get('name')
                ts_platform = ts.get('platform')
                saved_metrics[(ts_name, ts_platform)] = d

        for instance in self.instances.values():
            mkey = (instance.testsuite.name, instance.platform.name)
            if mkey not in saved_metrics:
                continue
            sm = saved_metrics[mkey]
            for metric, mtype, lower_better in interesting_metrics:
                if metric not in instance.metrics:
                    continue
                if sm[metric] == "":
                    continue
                delta = instance.metrics.get(metric, 0) - mtype(sm[metric])
                if delta == 0:
                    continue
                results.append((instance, metric, instance.metrics.get(metric, 0), delta,
                                lower_better))
        return results

    def footprint_reports(self, report, show_footprint, all_deltas,
                          footprint_threshold, last_metrics):
        if not report:
            return

        logger.debug("running footprint_reports")
        deltas = self.compare_metrics(report)
        warnings = 0
        if deltas and show_footprint:
            for i, metric, value, delta, lower_better in deltas:
                if not all_deltas and ((delta < 0 and lower_better) or
                                       (delta > 0 and not lower_better)):
                    continue

                percentage = 0
                if value > delta:
                    percentage = (float(delta) / float(value - delta))

                if not all_deltas and (percentage < (footprint_threshold / 100.0)):
                    continue

                logger.info("{:<25} {:<60} {}{}{}: {} {:<+4}, is now {:6} {:+.2%}".format(
                    i.platform.name, i.testsuite.name, Fore.YELLOW,
                    "INFO" if all_deltas else "WARNING", Fore.RESET,
                    metric, delta, value, percentage))
                warnings += 1

        if warnings:
            logger.warning("Deltas based on metrics from last %s" %
                           ("release" if not last_metrics else "run"))

    def summary(self, results, unrecognized_sections, duration):
        failed = 0
        run = 0
        for instance in self.instances.values():
            if instance.status == "failed":
                failed += 1
            elif instance.metrics.get("unrecognized") and not unrecognized_sections:
                logger.error("%sFAILED%s: %s has unrecognized binary sections: %s" %
                             (Fore.RED, Fore.RESET, instance.name,
                              str(instance.metrics.get("unrecognized", []))))
                failed += 1

            # FIXME: need a better way to identify executed tests
            handler_time = instance.metrics.get('handler_time', 0)
            if float(handler_time) > 0:
                run += 1

        if results.total and results.total != results.skipped_configs:
            pass_rate = (float(results.passed) / float(results.total - results.skipped_configs))
        else:
            pass_rate = 0

        logger.info(
            "{}{} of {}{} test configurations passed ({:.2%}), {}{}{} failed, {} skipped with {}{}{} warnings in {:.2f} seconds".format(
                Fore.RED if failed else Fore.GREEN,
                results.passed,
                results.total,
                Fore.RESET,
                pass_rate,
                Fore.RED if results.failed else Fore.RESET,
                results.failed + results.error,
                Fore.RESET,
                results.skipped_configs,
                Fore.YELLOW if self.plan.warnings else Fore.RESET,
                self.plan.warnings,
                Fore.RESET,
                duration))

        total_platforms = len(self.platforms)
        # if we are only building, do not report about tests being executed.
        if self.platforms and not self.env.options.build_only:
            logger.info("In total {} test cases were executed, {} skipped on {} out of total {} platforms ({:02.2f}%)".format(
                results.cases - results.skipped_cases,
                results.skipped_cases,
                len(self.filtered_platforms),
                total_platforms,
                (100 * len(self.filtered_platforms) / len(self.platforms))
            ))

        built_only = results.total - run - results.skipped_configs
        logger.info(f"{Fore.GREEN}{run}{Fore.RESET} test configurations executed on platforms, \
{Fore.RED}{built_only}{Fore.RESET} test configurations were only built.")

    def save_reports(self, name, suffix, report_dir, no_update, platform_reports):
        if not self.instances:
            return

        logger.info("Saving reports...")
        if name:
            report_name = name
        else:
            report_name = "twister"

        if report_dir:
            os.makedirs(report_dir, exist_ok=True)
            filename = os.path.join(report_dir, report_name)
            outdir = report_dir
        else:
            outdir = self.outdir
            filename = os.path.join(outdir, report_name)

        if suffix:
            filename = "{}_{}".format(filename, suffix)

        if not no_update:
            json_file = filename + ".json"
            self.json_report(json_file, version=self.env.version)
            self.xunit_report(json_file, filename + ".xml", full_report=False)
            self.xunit_report(json_file, filename + "_report.xml", full_report=True)
            self.xunit_report_suites(json_file, filename + "_suite_report.xml")

            if platform_reports:
                self.target_report(json_file, outdir, suffix)


    def target_report(self, json_file, outdir, suffix):
        platforms = {inst.platform.name for _, inst in self.instances.items()}
        for platform in platforms:
            if suffix:
                filename = os.path.join(outdir,"{}_{}.xml".format(platform, suffix))
            else:
                filename = os.path.join(outdir,"{}.xml".format(platform))
            self.xunit_report(json_file, filename, platform, full_report=True)
