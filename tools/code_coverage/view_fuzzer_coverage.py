#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a local HTML report of the ClusterFuzz explorations
by a given fuzzer.

  * Example usage: view_fuzz_coverage.py --fuzzer my_fuzzer_binary
"""

import argparse
import os
import subprocess
import sys
import tempfile
import pathlib

script_dir = os.path.dirname(os.path.realpath(__file__))
chromium_src_dir = os.path.dirname(os.path.dirname(script_dir))

# These may evolve over time, so if this script doesn't work, you may
# need to adjust these. In an ideal world we'd look these up from LUCI
# infrastructure but we're intentionally making a local script somewhat
# equivalentt to LUCI infrastructure, so for now let's not rely on that.
gn_args = """
dcheck_always_on = false
enable_mojom_fuzzer = true
ffmpeg_branding = "ChromeOS"
is_component_build = false
is_debug = false
pdf_enable_xfa = true
proprietary_codecs = true
use_clang_coverage = true
use_libfuzzer = true
use_remoteexec = true
symbol_level = 2
"""


def _ParseCommandArguments():
  """Adds and parses relevant arguments for tool comands.

  Returns:
    A dictionary representing the arguments.
  """
  arg_parser = argparse.ArgumentParser()
  arg_parser.usage = __doc__

  arg_parser.add_argument('--fuzzer',
                          required=True,
                          type=str,
                          help='Fuzzer binary name.')
  arg_parser.add_argument('--build-dir',
                          default=os.path.join(chromium_src_dir, 'out',
                                               'coverage'),
                          help='Where to build fuzzers.')
  arg_parser.add_argument('--html-dir',
                          default=os.path.join(chromium_src_dir, 'out',
                                               'coverage-html'),
                          help='Where to put HTML report.')
  arg_parser.add_argument(
      '--retain-build-dir',
      action='store_true',
      help=
      'Avoid cleaning the build dir (may result in multiple fuzzers being analyzed).'
  )
  args = arg_parser.parse_args()
  return args


def step(name):
  """Print a banner for the upcoming task.."""
  print("==== " + name + " ====:")


def check_call(args, *, cwd=None, shell=False):
  """Equivalent to subprocess.check_call but logs command."""
  print(" ".join(args))
  subprocess.check_call(args, cwd=cwd, shell=shell)


def Main():
  args = _ParseCommandArguments()

  os.makedirs(args.build_dir, exist_ok=True)
  os.makedirs(args.html_dir, exist_ok=True)

  step("Writing gn args")
  gn_args_file = os.path.join(args.build_dir, "args.gn")
  with open(gn_args_file, "w") as f:
    f.write(gn_args)

  if not args.retain_build_dir:
    step("gn clean")
    check_call(["gn", "clean", args.build_dir], cwd=chromium_src_dir)
  step("gn gen")
  check_call(["gn", "gen", args.build_dir], cwd=chromium_src_dir)
  step("autoninja")
  check_call(["autoninja", "-C", args.build_dir, args.fuzzer])
  corpora_dir = tempfile.TemporaryDirectory()
  step("Download corpora")
  check_call([
      sys.executable,
      os.path.join(script_dir, "download_fuzz_corpora.py"), "--download-dir",
      corpora_dir.name, "--build-dir", args.build_dir
  ])
  individual_profdata_dir = tempfile.TemporaryDirectory()
  step(
      "Running fuzzers (can take a while - NB you might need a valid DISPLAY set for some fuzzers)"
  )
  check_call([
      sys.executable,
      os.path.join(script_dir, "run_all_fuzzers.py"), "--fuzzer-binaries-dir",
      args.build_dir, "--fuzzer-corpora-dir", corpora_dir.name,
      "--profdata-outdir", individual_profdata_dir.name
  ])
  step("Merging profdata")
  merged_profdata_dir = tempfile.TemporaryDirectory()
  merged_profdata_file = os.path.join(merged_profdata_dir.name, "out.profdata")
  llvm_dir = os.path.join(chromium_src_dir, "third_party", "llvm-build",
                          "Release+Asserts", "bin")
  check_call([
      sys.executable,
      os.path.join(script_dir, "merge_all_profdata.py"), "--profdata-dir",
      individual_profdata_dir.name, "--outfile", merged_profdata_file,
      "--llvm-profdata",
      os.path.join(llvm_dir, "llvm-profdata")
  ])
  step("Generating HTML")
  check_call([
      os.path.join(llvm_dir, "llvm-cov"), "show", args.fuzzer, "-format=html",
      "-instr-profile", merged_profdata_file, "-output-dir", args.html_dir
  ],
             cwd=args.build_dir)
  uri = pathlib.Path(os.path.join(args.html_dir, "index.html")).as_uri()
  print("Report URI " + uri)
  step("Opening HTML in Chrome")
  check_call(["google-chrome-stable", uri], shell=True)


if __name__ == '__main__':
  sys.exit(Main())
