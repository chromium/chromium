#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Script that compares the metrics when running Autofill captured site tests
# with some features enabled and disabled.
# Run from the root of the Chromium src directory. -h for help.
# The tool expects that the captured_sites_interactive_tests binary is built.
import argparse, json, os, urllib.parse


# Extracts the names of all non-disabled captured site tests.
def get_all_sites():
  with open("chrome/test/data/autofill/captured_sites/artifacts/testcases.json",
            "r") as f:
    return (site["site_name"] for site in json.loads(f.read())["tests"]
            if not site.get("disabled", False))

# Command line args.
parser = argparse.ArgumentParser(epilog="List of tests: " +
                                 ", ".join(get_all_sites()))
parser.add_argument(
    "target",
    help=("Build target of captured_sites_interactive_tests binary. "
          "For example, Default. The binary should already exist."))
parser.add_argument("features", help="A comma-separated list of feature names.")
parser.add_argument(
    "-r",
    dest="histogram_regex",
    help=("A regex matching the histogram names that should be dumped. If not "
          "specified, the metrics of all histograms dumped."))
parser.add_argument(
    "-o",
    dest="output",
    default="/tmp/",
    help=("Directory to record the metrics into. Creates files per test, named"
          " after the test case."))
parser.add_argument(
    "-t",
    dest="test",
    help="Test case. If no test is specified, all tests are run.")
parser.add_argument("-s",
                    dest="silent",
                    action="store_true",
                    help="Don't print test output.")
args = parser.parse_args()

# The captured_sites_interactive_tests binary should be built.
captured_site_tests = "out/%s/captured_sites_interactive_tests" % args.target
assert os.path.exists(captured_site_tests)


# Runs the capture site test for `site` and scrapes the metrics.
# args.features is enabled/disabled depending on `features_enabled`.
def run_test(site, features_enabled):
  cmd = "./" + captured_site_tests
  cmd += (" --gtest_filter="
          "All/AutofillCapturedSitesInteractiveTest.Recipe/" + site)
  # Enable scraping tools. Special characters need to be escaped.
  cmd += " --enable-features=AutofillCapturedSiteTestsMetricsScraper"
  cmd += ":output_dir/" + urllib.parse.quote(args.output, safe="")
  if args.histogram_regex is not None:
    cmd += "/histogram_regex/" + urllib.parse.quote(args.histogram_regex,
                                                    safe="")
  # En- or disable features.
  if features_enabled:
    cmd += "," + args.features
  else:
    cmd += " --disable-features=" + args.features
  # Random arguments that the captured site tests recommend.
  cmd += " --enable-pixel-output-in-tests"
  cmd += " --test-launcher-timeout=10000000"
  cmd += " --ui-test-action-max-timeout=10000000"
  cmd += (" --vmodule=captured_sites_test_utils=2\,"
          "autofill_captured_sites_interactive_uitest=1")
  # Maybe disable output.
  if args.silent:
    cmd += " > /dev/null 2>&1"
  # Run
  os.system(cmd)


# Runs the captured site test `site` twice. Once with `args.features` enabled
# and once with the feature disabled.
# Diffs the metrics collected.
def run_tests_and_diff(site):
  print("Testing %s..." % site)

  def file_name(infix):
    return "%s/%s%s.txt" % (args.output, site, infix)

  # `output` is where the captured site test will write to. Rename the file
  # afterwards to distinguish between enabled/disabled state.
  output = file_name("")
  result_enabled = file_name("_enabled")
  result_disabled = file_name("_disabled")

  print("Running with features enabled. Results at " + result_enabled)
  run_test(site, True)
  os.rename(output, result_enabled)

  print("Running with features disabled. Results at " + result_disabled)
  run_test(site, False)
  os.rename(output, result_disabled)

  print("Comparing metrics (no output means no diff)")
  os.system("diff %s %s" % (result_enabled, result_disabled))
  print("")

# If a test is specified, only run that specific test. Otherwise run all.
if args.test is None:
  for site in get_all_sites():
    run_tests_and_diff(site)
else:
  run_tests_and_diff(args.test)
