#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import typing
import os
import datetime

from driver import DriverContext
import scenarios
import browsers
import utils


def IterScenarios(
    scenario_names: typing.List[str],
    browser_driver_factory: typing.Callable[[], browsers.BrowserDriver],
    **kwargs):
  for scenario_and_browser_name in scenario_names:
    scenario_name, _, browser_name = scenario_and_browser_name.partition(':')
    browser_name, _, variation = browser_name.partition(':')
    browser_driver = browser_driver_factory(browser_name, variation)
    scenario_driver = scenarios.MakeScenarioDriver(scenario_name,
                                                   browser_driver, **kwargs)
    if scenario_driver is None:
      logging.error(f"Skipping invalid scenario {scenario_and_browser_name}.")
    else:
      yield scenario_driver


def main():
  parser = argparse.ArgumentParser(description='Runs browser power benchmarks')
  parser.add_argument("--output_dir", help="Output dir")
  parser.add_argument('--no-checks',
                      dest='no_checks',
                      action='store_true',
                      help="Invalid environment doesn't throw")
  mode_group = parser.add_mutually_exclusive_group()
  mode_group.add_argument(
      '--tracing_mode',
      dest='tracing_mode',
      action='store_true',
      help="Grab a trace instead of a profile or benchmark.")

  # Profile related arguments
  mode_group.add_argument(
      '--profile_mode',
      dest='profile_mode',
      action='store',
      choices=["wakeups", "cpu_time"],
      help="Profile the application in one of two modes: wakeups, cpu_time.")
  parser.add_argument("--power_sampler",
                      help="Path to power sampler binary",
                      required=True)
  parser.add_argument(
      '--scenarios',
      dest='scenarios',
      action='store',
      required=True,
      nargs='+',
      help="List of scenarios and browsers to run in the format"
      "<scenario_name>:<browser_name>, e.g. idle_on_wiki:safari")
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

  parser.add_argument('--extra-command-line',
                      dest='extra_command_line',
                      action='store')

  args = parser.parse_args()

  if args.verbose:
    log_level = logging.DEBUG
  else:
    log_level = logging.INFO
  logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)

  output_dir = args.output_dir
  if not output_dir:
    output_dir = os.path.join("output",
                              datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))

  logging.info(f'Outputing results in {os.path.abspath(output_dir)}')
  with DriverContext(output_dir, args.power_sampler) as driver:
    driver.CheckEnv(not args.no_checks)
    # This is the average brightness from UMA data.
    driver.SetMainDisplayBrightness(65)

    driver.WaitBatteryNotFull()

    # Measure or Profile all defined scenarios.
    def BrowserFactory(browser_name, variation):
      return browsers.MakeBrowserDriver(
          browser_name,
          variation,
          chrome_user_dir=args.chrome_user_dir,
          output_dir=output_dir,
          tracing_mode=args.tracing_mode,
          extra_command_line=args.extra_command_line)

    for scenario in IterScenarios(args.scenarios,
                                  BrowserFactory,
                                  meet_meeting_id=args.meet_meeting_id):

      if args.tracing_mode:
        logging.info(f'Tracing scenario {scenario.name} ...')
        driver.Trace(scenario)
      elif args.profile_mode:
        logging.info(f'Profiling scenario {scenario.name} ...')
        driver.Profile(scenario, profile_mode=args.profile_mode)
      else:
        logging.info(f'Recording scenario {scenario.name} ...')
        driver.Record(scenario)


if __name__ == "__main__":
  main()
