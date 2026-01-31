#!/usr/bin/env vpython3

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import io
import os
import platform
import subprocess
import sys


# Set up path to load action_helpers.py which enables us to do
# atomic output that's maximally compatible with ninja.
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir,
                 os.pardir, os.pardir, os.pardir, 'build'))
import action_helpers


def calculate_cxxbridge_args(unrecognized_script_args, rustc_print_cfg_path):
  """ Calculates and returns command-line flags for `cxxbridge`.

      `unrecognized_script_args` should be post-`--` args that are not explictly
      named and handled by `argparse` in `main`.  These are (in order):

      1. `rustc` flags / `"{{rustflags}}"` expansion from `rust_cxx.gni`:
          1.1. `--cfg=foo` needs to be transformed into `--cfg=foo=true`
               (there is also special handling for `--cfg=foo=false`)
          1.2. All other `rustc` flags (e.g. `-Cpanic=abort`) are ignored
      2. "RUSTFLAGS_SEPARATOR"
      3. `cxxbridge` flags (e.g. source files and/or `--cxx-impl-annotations`;
         we forward all of them)

      Returns command-line flags to pass to `cxxbridge`.
  """

  rustflags_separator = unrecognized_script_args.index("RUSTFLAGS_SEPARATOR")
  cxxbridge_args = unrecognized_script_args[rustflags_separator+1:]

  parser = argparse.ArgumentParser("<def filter_non_cfg_rustflags>")
  parser.add_argument("--cfg", action="append"),
  parsed_rustc_args, _other_args = parser.parse_known_args(
      args=unrecognized_script_args[:rustflags_separator])
  for cfg_arg in parsed_rustc_args.cfg:
    if "=" not in cfg_arg:  # `buildflag_header.gni` only supports bool flags.
      # TODO(https://crbug.com/436606652): Stop using `_BUILDFLAG_NOT_SET_`
      # prefix and instead use `--check-cfg` in `run_cxxbridge.py` to
      # detect buildflag names.
      FALSE_PREFIX = "_BUILDFLAG_NOT_SET_"
      if cfg_arg.startswith(FALSE_PREFIX):
        flagname = cfg_arg[len(FALSE_PREFIX):]
        cxxbridge_args.append(f"--cfg={flagname}=false")
      else:
        flagname = cfg_arg
        cxxbridge_args.append(f"--cfg={flagname}=true")

  with open(rustc_print_cfg_path, 'r') as file:
    for line in file:
      line = line.strip()
      cxxbridge_args.append(f"--cfg={line}")

  return cxxbridge_args


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
  parser.add_argument('--rustc-print-cfg-path', required=True,
                      help='path to output from //build/rust/gni_impl:rustc_print_cfg')
  parser.add_argument('args',
                      metavar='args',
                      nargs='+',
                      help="Args to pass through")
  args = parser.parse_args()
  cxxbridge_args = calculate_cxxbridge_args(args.args, args.rustc_print_cfg_path)
  v = run(args.exe, cxxbridge_args, args.cc, False)
  if v != 0:
    return v
  return run(args.exe, cxxbridge_args, args.header, True)


if __name__ == '__main__':
  sys.exit(main())
