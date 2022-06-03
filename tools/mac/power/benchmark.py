#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import psutil
import signal
import subprocess
import sys
import time

import utils
from driver import Driver
import generate_scripts


def SignalHandler(sig, frame, driver):
  """Handle the run being aborted.
  """
  driver.Teardown()
  sys.exit(0)


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

  if args.profile_mode and args.run_measure:
    logging.error("Cannot measure and profile at the same time, choose one.")
    sys.exit(-1)

  # Generate the runner scripts
  extra_args = {}
  if args.meet_meeting_id:
    extra_args["meeting_id"] = args.meet_meeting_id
  generate_scripts.generate_all(extra_args)

  driver = Driver(args.output_dir)
  driver.CheckEnv(not args.no_checks)

  signal.signal(
      signal.SIGINT, lambda sig, frame: SignalHandler(sig, frame, driver))

  if args.chrome_user_dir:
    chrome_extra_arg = "--user-data-dir=%s" % args.chrome_user_dir
  else:
    chrome_extra_arg = "--guest"

  # Measure or Profile all defined scenarios. To add/remove some change their
  # "skip" attribute in utils.SCENARIOS.
  for scenario in utils.SCENARIOS:

    if scenario["browser"] != "Safari":
      scenario["extra_args"].append(chrome_extra_arg)

    # TODO(crbug.com/1224994): Allow scenario filtering like gtest_filter.
    if not scenario["skip"]:
      if args.run_measure:
        logging.info(f'Recording scenario {scenario["name"]} ...')
        driver.Record(scenario)

      if args.profile_mode:
        logging.info(f'Profiling scenario {scenario["name"]} ...')
        driver.Profile(scenario, profile_mode=args.profile_mode)


if __name__ == "__main__":
  main()
