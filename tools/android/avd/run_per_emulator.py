#!/usr/bin/env vpython3
#
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to run a command on all available emulators."""

import argparse
import logging
import os
import re
import subprocess
import sys
import time

_SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(_SRC_ROOT, 'third_party', 'catapult', 'devil'))
from devil.utils import logging_common

sys.path.append(os.path.join(_SRC_ROOT, 'tools', 'android', 'avd'))
from avd import get_avd_configs


def get_emulators(all_emulators):
  """Gets the list of emulators, optionally filtering by API level."""
  emulators = []
  for config in get_avd_configs():
    if not all_emulators and not config.IsAvailable():
      continue

    path = config.avd_proto_path
    basename = os.path.basename(path)
    api_level = 0
    # Try to extract API level like android_31_...
    match = re.search(r'android_(\d+)', basename)
    if match:
      api_level = int(match.group(1))
    else:
      # Try to extract API level like generic_android27.textpb
      match = re.search(r'generic_android(\d+)', basename)
      if match:
        api_level = int(match.group(1))

    # If we found an API level and it's 28 or less, skip it.
    if api_level and api_level <= 28:
      logging.info('Skipping emulator %s due to old API level (%d).', path,
                   api_level)
      continue
    emulators.append(path)
  return emulators


def wait_for_emulator(serial):
  """Waits for the emulator to be online."""
  logging.info('Waiting for emulator %s to be online...', serial)
  for _ in range(60):
    try:
      output = subprocess.check_output(['adb', 'devices'], encoding='utf-8')
      if re.search(r'^%s\s+device$' % re.escape(serial), output, re.MULTILINE):
        logging.info('Emulator %s is online.', serial)
        return True
    except subprocess.CalledProcessError:
      pass
    time.sleep(1)
  logging.error('Emulator %s did not come online.', serial)
  return False


def main():
  """Starts each emulator, runs a command, and stops it."""
  parser = argparse.ArgumentParser(
      description='Run a command on all available emulators.')
  parser.add_argument('--cmd',
                      required=True,
                      help='The command to run on the emulator.')
  parser.add_argument('--all',
                      action='store_true',
                      help='Run on all emulators, not just installed ones.')
  parser.add_argument('--output-file', help='File to write the output to.')
  logging_common.AddLoggingArguments(parser)
  args = parser.parse_args()

  logging_common.InitializeLogging(args)

  if args.output_file:
    output_dir = os.path.dirname(args.output_file)
    if not os.path.exists(output_dir):
      os.makedirs(output_dir)
    # Clear the file if it exists
    open(args.output_file, 'w').close()

  emulators = get_emulators(args.all)
  if not emulators:
    logging.warning('No available emulators found.')
    return

  for emulator_config in emulators:
    logging.info('Starting emulator: %s', emulator_config)
    try:
      start_process = subprocess.Popen([
          'tools/android/avd/avd.py', 'start', '--avd-config', emulator_config
      ],
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE,
                                       encoding='utf-8')
      stdout, stderr = start_process.communicate()
      if start_process.returncode != 0:
        logging.error('Failed to start emulator %s: %s', emulator_config,
                      stderr)
        continue

      # The serial number is expected to be in the output of the start command.
      match = re.search(r'emulator-\d+', stdout)
      if not match:
        logging.error('Could not determine serial for emulator %s',
                      emulator_config)
        continue
      serial = match.group(0)

      if wait_for_emulator(serial):
        logging.info('Running command on %s: %s', serial, args.cmd)
        try:
          cmd_output = subprocess.check_output(args.cmd,
                                               shell=True,
                                               encoding='utf-8')
          if args.output_file:
            with open(args.output_file, 'a') as f:
              f.write(f'{emulator_config}: {cmd_output.strip()}\n')
          else:
            print(f'Output for {emulator_config}:\n{cmd_output}')
        except subprocess.CalledProcessError as e:
          logging.error('Command failed on %s: %s', emulator_config, e)

      logging.info('Stopping emulator: %s', emulator_config)
      subprocess.run(
          ['tools/android/avd/avd.py', 'stop', '--avd-config', emulator_config],
          check=False)
    except Exception as e:
      logging.error('An error occurred with emulator %s: %s', emulator_config,
                    e)
      # Ensure the emulator is stopped even if an error occurs.
      subprocess.run(
          ['tools/android/avd/avd.py', 'stop', '--avd-config', emulator_config],
          check=False)


if __name__ == '__main__':
  main()
