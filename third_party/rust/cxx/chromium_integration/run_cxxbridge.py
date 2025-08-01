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


# TODO(https://crbug.com/435437947): Deduplicate the following pieces which
# also appear in `//build/rust/run_build_script.py`:
# * `--rust-prefix` and `--target` args
# * `rustc_name` (note that `build/rust/rustc_wrapper.py` gets `--rustc` with
#   the full path so it doesn't need to worry about the `.exe` suffix;  maybe
#   this aspect can also be unified somehow)
# * `host_triple`
# * Parts of `get_cfg_args` below duplicate some parts of
#   `set_cargo_cfg_target_env_variables` from `run_build_script.py`


def rustc_name():
  if platform.system() == 'Windows':
    return "rustc.exe"
  else:
    return "rustc"


def host_triple(rustc_path):
  """ Works out the host rustc target. """
  args = [rustc_path, "-vV"]
  known_vars = {}
  output = subprocess.check_output(args).decode('utf-8')
  for line in output.splitlines():
    m = RUSTC_VERSION_LINE.match(line.rstrip())
    if m:
      known_vars[m.group(1)] = m.group(2)
  return known_vars["host"]


def get_cfg_args(rust_prefix, target):
  """ Returns `--cfg=target_arch=...` etc. based on output from rustc. """

  rustc_path = os.path.join(rust_prefix, rustc_name())
  if target is None:
    target = host_triple(rustc_path)

  # TODO(lukasza): Check if command-line flags other `--target` may affect the
  # output of `--print-cfg`.  If so, then consider also passing extra `args`
  # (derived from `rustflags` maybe?).
  args = [rustc_path, "--print=cfg", f"--target={target}"]

  # TODO(https://crbug.com/435437947): Ideally `rustc --print=cfg
  # --target=...` would only be invoked **once** per build (not once per
  # `run_cxxbridge.py` and once per `run_build_script.py`).
  result = []
  output = subprocess.check_output(args).decode('utf-8')
  for line in output.splitlines():
    line = line.strip()

    # TODO(https://crbug.com/435437947): To unblock https://crrev.com/c/6797874
    # we don't have to handle `unix` or `windows` or `debug_assertions`, but we
    # should figure out how to do it in the long-term.  The problem is that in
    # the _absence_ of `unix` we need to set `unix=false` (and the same for all
    # the other _known_ [= hardcoded?] no-`=` conditions).
    if "=" not in line: continue

    result.append(f"--cfg={line}")

  return result


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
  parser.add_argument('--target', help='rust target triple')
  parser.add_argument('--rust-prefix', required=True, help='rust path prefix')
  parser.add_argument('args',
                      metavar='args',
                      nargs='+',
                      help="Args to pass through")
  args = parser.parse_args()
  args.args += get_cfg_args(args.rust_prefix, args.target)
  v = run(args.exe, args.args, args.cc, False)
  if v != 0:
    return v
  return run(args.exe, args.args, args.header, True)


if __name__ == '__main__':
  sys.exit(main())
