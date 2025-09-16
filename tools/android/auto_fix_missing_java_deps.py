#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import pathlib
import shlex
import subprocess
import sys
import logging

_SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.append(str(_SRC_ROOT / 'build/android'))
from pylib import constants


def main():
  parser = argparse.ArgumentParser(
      description='Continuously build and apply GN deps fixes.')
  parser.add_argument('-C',
                      '--output-directory',
                      help='Path to the root build directory.')
  parser.add_argument('--quiet',
                      action='store_true',
                      help='Pass --quiet to autoninja')
  parser.add_argument('targets', nargs='*', help='Targets to build')

  logging.basicConfig(level=logging.INFO,
                      format='\033[33mStatus:\033[0m %(message)s')
  args = parser.parse_args()
  if args.output_directory:
    constants.SetOutputDirectory(args.output_directory)
  constants.CheckOutputDirectory()
  output_dir = constants.GetOutDirectory()

  cmd = ['autoninja', '-C', output_dir, '-config', 'no-remote-javac']
  if args.quiet:
    cmd += ['--quiet']
  cmd += args.targets

  for iteration_count in range(1, 10000):
    logging.info('Building Iteration %d', iteration_count)
    result = subprocess.run(cmd)

    if result.returncode == 0:
      logging.info('Build successful after %d iteration(s).', iteration_count)
      return 0

    siso_output_path = os.path.join(output_dir, 'siso_output')
    if not os.path.exists(siso_output_path):
      logging.error('siso_output not found at %s', siso_output_path)
      return 1

    with open(siso_output_path, 'r') as f:
      commands = [
          l.strip() for l in f if l.strip().startswith('build/gn_editor')
      ]

    if not commands:
      logging.error('Build failed and no gn_editor hints were found.')
      return 1

    for command in commands:
      logging.info('Running: %s', command)
      cmd_parts = shlex.split(command)
      subprocess.run(cmd_parts, check=True, cwd=_SRC_ROOT)

  logging.info('Still not fixed after %d iterations. Giving up.',
               iteration_count)
  return 1


if __name__ == '__main__':
  sys.exit(main())
