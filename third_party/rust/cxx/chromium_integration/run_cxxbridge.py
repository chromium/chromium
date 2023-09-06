#!/usr/bin/env vpython3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

# Set up path to load action_helpers.py which enables us to do
# atomic output that's maximally compatible with ninja.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, os.pardir, os.pardir, 'build'))
import action_helpers


def run(exe, args, output, is_header):
  cmdargs = [os.path.abspath(exe)]
  cmdargs.extend(args)
  if is_header:
    cmdargs.extend(["--header"])
  job = subprocess.run(cmdargs, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  messages = job.stderr.decode('utf-8')
  if messages.rstrip():
    print(messages, file=sys.stderr)
  if job.returncode != 0:
    return job.returncode
  with action_helpers.atomic_output(output) as output:
    output.write(job.stdout)
  return 0


def main():
  parser = argparse.ArgumentParser("run_cxxbridge.py")
  parser.add_argument("--exe", help="Path to cxxbridge", required=True),
  parser.add_argument("--cc", help="output cc file", required=True)
  parser.add_argument("--header", help="output h file", required=True)
  parser.add_argument('args',
                      metavar='args',
                      nargs='+',
                      help="Args to pass through")
  args = parser.parse_args()
  v = run(args.exe, args.args, args.cc, False)
  if v != 0:
    return v
  return run(args.exe, args.args, args.header, True)


if __name__ == '__main__':
  sys.exit(main())
