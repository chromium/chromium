#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides a local HTML report of the ClusterFuzz explorations
by a given fuzzer.

  * Example usage: vpython3 view_fuzz_coverage.py --target my_fuzzer_binary
    --fuzzer-type libfuzzer
"""

import argparse
import os
import subprocess
import sys
import tempfile
import pathlib

script_dir = os.path.dirname(os.path.realpath(__file__))
chromium_src_dir = os.path.dirname(os.path.dirname(script_dir))

XVFB_PATH = os.path.join(chromium_src_dir, 'testing/xvfb.py')

# These may evolve over time, so if this script doesn't work, you may
# need to adjust these. In an ideal world we'd look these up from LUCI
# infrastructure but we're intentionally making a local script somewhat
# equivalent to LUCI infrastructure, so for now let's not rely on that.
gn_args_libfuzzer = """
dcheck_always_on = false
enable_mojom_fuzzer = true
ffmpeg_branding = "ChromeOS"
is_component_build = false
is_debug = false
pdf_enable_xfa = true
proprietary_codecs = true
use_clang_coverage = true
use_clang_modules = false
use_libfuzzer = true
use_remoteexec = true
symbol_level = 2
"""

gn_args_blackbox = """
dcheck_always_on = false
is_asan = true
is_component_build = false
is_debug = false
use_clang_coverage = true
use_clang_modules = false
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

  arg_parser.add_argument(
      '--fuzzer-type',
      choices=['libfuzzer', 'centipede', 'fuzzilli', 'blackbox'],
      default='libfuzzer',
      help='The type of fuzzer to analyze.')
  arg_parser.add_argument('--target',
                          type=str,
                          required=True,
                          help='The fuzzer binary name. For blackbox fuzzers, '
                          'the target binary to use, e.g. Chrome or v8')
  arg_parser.add_argument('--build-dir',
                          default=os.path.join(chromium_src_dir, 'out',
                                               'coverage'),
                          help='Where to build fuzzers.')
  arg_parser.add_argument('--html-dir',
                          default=os.path.join(chromium_src_dir, 'out',
                                               'coverage-html'),
                          help='Where to put HTML report.')
  arg_parser.add_argument(
      '--corpora-dir',
      help='The directory containing the fuzzing corpora. Required when the '
      'fuzzer type is blackbox, where it is the directory of html or js files '
      'to be run with the target.')
  arg_parser.add_argument(
      '--retain-build-dir',
      action='store_true',
      help='Avoid cleaning the build dir (may result in multiple fuzzers being '
      'analyzed).')
  arg_parser.add_argument(
      '--testcase-timeout',
      type=int,
      default=60,
      help='Timeout in seconds for each testcase. Defaults to 60 seconds. For '
      'blackbox fuzzers, this corresponds to the TEST_TIMEOUT env variable for '
      'the bot running the fuzzer. Many blackbox fuzzers require a timeout '
      'because test cases can\'t signal when they are finished.')
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
    if args.fuzzer_type == 'blackbox':
      f.write(gn_args_blackbox)
    else:
      f.write(gn_args_libfuzzer)

  if not args.retain_build_dir:
    step("gn clean")
    check_call(["gn", "clean", args.build_dir], cwd=chromium_src_dir)
  step("gn gen")
  check_call(["gn", "gen", args.build_dir], cwd=chromium_src_dir)

  step("autoninja")
  check_call(["autoninja", "-C", args.build_dir, args.target])

  is_blackbox_fuzzer = args.fuzzer_type == 'blackbox'

  if is_blackbox_fuzzer:
    if not args.corpora_dir:
      print("--corpora-dir is required for blackbox fuzzer coverage.")
      return 1

    corpora_dir_path = args.corpora_dir
  else:
    temp_corpora_dir = tempfile.TemporaryDirectory()
    corpora_dir_path = temp_corpora_dir.name
    step("Download corpora")
    check_call([
        sys.executable,
        os.path.join(script_dir, "download_fuzz_corpora.py"), "--download-dir",
        corpora_dir_path, "--build-dir", args.build_dir, "--corpora-type",
        args.fuzzer_type
    ])

  individual_profdata_dir = tempfile.TemporaryDirectory()
  step('Running fuzzers (can take a while - NB you might need a valid DISPLAY '
       'set for some fuzzers)')

  run_all_fuzzers_cmd = [
      sys.executable, XVFB_PATH,
      os.path.join(script_dir, "run_all_fuzzers.py"), "--fuzzer-binaries-dir",
      args.build_dir, "--fuzzer-corpora-dir", corpora_dir_path,
      "--profdata-outdir", individual_profdata_dir.name, "--fuzzer",
      args.fuzzer_type, "--testcase-timeout",
      str(args.testcase_timeout)
  ]
  if is_blackbox_fuzzer:
    run_all_fuzzers_cmd += ["--target", args.target]

  check_call(run_all_fuzzers_cmd)

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
  target_path = os.path.join(args.build_dir, args.target)
  check_call([
      os.path.join(llvm_dir, "llvm-cov"), "show", target_path, "--format=html",
      "--instr-profile", merged_profdata_file, "--output-dir", args.html_dir,
      "--compilation-dir", args.build_dir
  ])

  uri = pathlib.Path(os.path.abspath(os.path.join(args.html_dir,
                                                  "index.html"))).as_uri()
  print("Report URI " + uri)
  step("Opening HTML in Chrome")
  check_call(["google-chrome-stable", uri])


if __name__ == '__main__':
  sys.exit(Main())
