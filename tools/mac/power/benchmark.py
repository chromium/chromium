#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import sys
import typing

from driver import Driver
import scenarios
import browsers


def IterScenarios(scenario_names: typing.List[str],
                  browser_driver: browsers.BrowserDriver, **kwargs):
  for scenario_name in scenario_names:
    scenario_driver = scenarios.MakeScenarioDriver(scenario_name,
                                                   browser_driver, **kwargs)
    if scenario_driver is None:
      logging.error(f"Skipping invalid scenario {scenario_name}.")
    else:
      yield scenario_driver


def IterBrowsers(browser_names: typing.List[str], **kwargs):
  for browser_name in browser_names:
    scenario_driver = browsers.MakeBrowserDriver(browser_name, **kwargs)
    if scenario_driver is None:
      logging.error(f"Skipping invalid browser {browser_name}.")
    else:
      yield scenario_driver


def main():
  parser = argparse.ArgumentParser(description='Runs browser power benchmarks')
  parser.add_argument("output_dir", help="Output dir")
  parser.add_argument('--no-checks',
                      dest='no_checks',
                      action='store_true',
                      help="Invalid environment doesn't throw")
  parser.add_argument(
      '--measure',
      dest='run_measure',
      action='store_true',
      help="Run measurements of the cpu use of the application.")

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
                      help='List of scenarios to run.')
  parser.add_argument('--browsers',
                      dest='browsers',
                      action='store',
                      required=True,
                      nargs='+',
                      help='List of browsers to run scenarios with.')
  parser.add_argument('--meet-meeting-id',
                      dest='meet_meeting_id',
                      action='store',
                      help='The meeting ID to use for the Meet benchmarks')
  parser.add_argument(
      '--chrome-user-dir',
      dest='chrome_user_dir',
      action='store',
      help='The user data dir to pass to Chrome via --user-data-dir')

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

  if not args.profile_mode and not args.run_measure:
    logging.error("One of measure or profile mode must be provided.")
    sys.exit(-1)
  if args.profile_mode and args.run_measure:
    logging.error("Cannot measure and profile at the same time, choose one.")
    sys.exit(-1)

  driver = Driver(args.output_dir)
  driver.CheckEnv(not args.no_checks)

  # Measure or Profile all defined scenarios.
  for browser in IterBrowsers(args.browsers,
                              chrome_user_dir=args.chrome_user_dir):
    for scenario in IterScenarios(args.scenarios,
                                  browser,
                                  meet_meeting_id=args.meet_meeting_id):
      if args.run_measure:
        logging.info(f'Recording scenario {scenario.name} ...')
        driver.Record(scenario)

      if args.profile_mode:
        logging.info(f'Profiling scenario {scenario.name} ...')
        driver.Profile(scenario, profile_mode=args.profile_mode)


if __name__ == "__main__":
  main()
