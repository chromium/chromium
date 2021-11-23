#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import typing
import os

from driver import Driver
import scenarios
import browsers


def IterScenarios(
    scenario_names: typing.List[str],
    browser_driver_factory: typing.Callable[[], browsers.BrowserDriver],
    **kwargs):
  for scenario_and_browser_name in scenario_names:
    scenario_name, _, browser_name = scenario_and_browser_name.partition(':')
    browser_driver = browser_driver_factory(browser_name)
    scenario_driver = scenarios.MakeScenarioDriver(scenario_name,
                                                   browser_driver, **kwargs)
    if scenario_driver is None:
      logging.error(f"Skipping invalid scenario {scenario_and_browser_name}.")
    else:
      yield scenario_driver


def main():
  parser = argparse.ArgumentParser(description='Runs browser power benchmarks')
  parser.add_argument("--output_dir",
                      help="Output dir",
                      action='store_true',
                      default="output")
  parser.add_argument('--no-checks',
                      dest='no_checks',
                      action='store_true',
                      help="Invalid environment doesn't throw")

  # Profile related arguments
  parser.add_argument(
      '--profile_mode',
      dest='profile_mode',
      action='store',
      choices=["wakeups", "cpu_time"],
      help="Profile the application in one of two modes: wakeups, cpu_time.")
  parser.add_argument('--scenarios',
                      dest='scenarios',
                      action='store',
                      required=True,
                      nargs='+',
                      help="List of scenarios and browsers to run in the format"
                      "<scenario_name>:<browser_name>.")
  parser.add_argument('--meet-meeting-id',
                      dest='meet_meeting_id',
                      action='store',
                      help='The meeting ID to use for the Meet benchmarks')
  parser.add_argument(
      '--chrome-user-dir',
      dest='chrome_user_dir',
      action='store',
      help='The user data dir to pass to Chrome via --user-data-dir')
  parser.add_argument('--chromium-path',
                      dest='chromium_path',
                      action='store',
                      help='The path to Chromium.app')

  parser.add_argument('--verbose',
                      action='store_true',
                      default=False,
                      help='Print verbose output.')

  args = parser.parse_args()

  if args.verbose:
    log_level = logging.INFO
  else:
    log_level = logging.WARNING
  logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)

  logging.info(f'Outputing results in {os.path.abspath(args.output_dir)}')
  driver = Driver(args.output_dir)
  driver.CheckEnv(not args.no_checks)

  # Measure or Profile all defined scenarios.
  browser_factory = lambda broeswer_name: browsers.MakeBrowserDriver(
      broeswer_name,
      chrome_user_dir=args.chrome_user_dir,
      chromium_path=args.chromium_path)
  for scenario in IterScenarios(args.scenarios,
                                browser_factory,
                                meet_meeting_id=args.meet_meeting_id):

    if args.profile_mode:
      logging.info(f'Profiling scenario {scenario.name} ...')
      driver.Profile(scenario, profile_mode=args.profile_mode)
    else:
      logging.info(f'Recording scenario {scenario.name} ...')
      driver.Record(scenario)


if __name__ == "__main__":
  main()
