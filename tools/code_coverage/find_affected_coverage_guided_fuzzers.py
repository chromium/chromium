#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import concurrent.futures
import json
import logging
import os
import subprocess
import sys
from typing import Dict, List, Set

script_dir = os.path.dirname(os.path.realpath(__file__))
chromium_src_dir = os.path.dirname(os.path.dirname(script_dir))

gn_args_libfuzzer = """
dcheck_always_on = false
enable_mojom_fuzzer = true
ffmpeg_branding = "ChromeOS"
is_component_build = true
is_debug = false
optimize_for_fuzzing = true
pdf_enable_xfa = true
proprietary_codecs = true
target_cpu = "x64"
target_os = "linux"
use_libfuzzer = true
use_reclient = false
use_remoteexec = true
use_siso = true
"""

# TODO: Support Centipede fuzzers too.
# https://buganizer.corp.google.com/issues/522382682


def generate_gn_build_dir(out_dir: str):
  """Generates the GN graph in the output directory."""
  logging.info(f'Generating GN graph in {out_dir}...')

  args_str = gn_args_libfuzzer.replace('\n', ' ')
  command = ['gn', 'gen', out_dir, f'--args={args_str}']

  try:
    subprocess.run(command, cwd=chromium_src_dir, check=True, env=os.environ)
  except subprocess.CalledProcessError as e:
    logging.error(f'Error running gn gen: {e}')
    raise


def run_gn_command(args: List[str]) -> List[str]:
  """Runs a gn command and returns stdout lines as a list."""
  try:
    command = ['gn'] + args
    logging.debug(f'Running command: {" ".join(command)}')
    # Explicitly pass os.environ to ensure that GN inherits crucial environment
    # variables set by swarming bots or the local environment (e.g., PATH,
    # DEPOT_TOOLS_PATH, toolchain paths, and RBE/Reclient variables). GN cannot
    # locate compilers, SDKs, or tools without these environment variables.
    result = subprocess.run(
      command,
      cwd=chromium_src_dir,
      capture_output=True,
      text=True,
      check=True,
      env=os.environ,
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]
  except subprocess.CalledProcessError as e:
    logging.error(
      f'Error running gn command: {e}\nStdout: {e.stdout}\nStderr: {e.stderr}'
    )
    raise
  except FileNotFoundError:
    logging.error(
      '\'gn\' command not found. Ensure depot_tools is in your PATH.'
    )
    raise


def find_all_fuzzer_targets(out_dir: str) -> List[str]:
  """Finds all fuzz targets which are executable that depend on
  :fuzzing_engine.
  """
  logging.info(f'Finding all fuzzer targets in {out_dir}...')
  args = [
    'refs',
    out_dir,
    '//testing/libfuzzer:fuzzing_engine',
    '--all',
    '--type=executable',
    '--as=label',
    '-q',
  ]
  return run_gn_command(args)


def find_reverse_deps(out_dir: str, gn_file_path: str) -> List[str]:
  """Finds all targets that depend on the given GN file path."""
  logging.debug(f'Finding reverse dependencies for {gn_file_path}...')
  args = ['refs', out_dir, '--all', '--as=label', '-q', gn_file_path]
  return run_gn_command(args)


def get_modified_files() -> List[str]:
  """Detects modified C/C++ files against the base revision."""
  # TODO: Address the issue that if a chain of CLs is uploaded
  # (HEAD -> A -> B -> C) and the CQ is run on C, this might discover
  # changes in A or B and then cause fuzzers to run, even if C didn't
  # have any relevant changes.
  # https://buganizer.corp.google.com/issues/522387539
  logging.info('Detecting modified files against HEAD~...')
  command = ['git', 'diff', '--name-only', '--diff-filter=d', 'HEAD~']
  result = subprocess.run(
    command, cwd=chromium_src_dir, capture_output=True, text=True, check=True
  )
  return [
    f.strip()
    for f in result.stdout.splitlines()
    if f.strip().endswith(('.cc', '.cpp', '.c', '.h', '.m', '.mm'))
  ]


def get_affected_fuzzers(
  modified_files: List[str], out_dir: str, all_fuzzers: List[str]
) -> Dict[str, List[str]]:
  """Maps modified files to the fuzzer targets they affect."""
  affected_map: Dict[str, List[str]] = {}
  all_fuzzers_set = set(all_fuzzers)

  def process_file(file_path: str):
    rel_path = file_path.replace(os.sep, '/')
    # Prefix with '//' so that file paths in the output map are formatted
    # consistently with GN target labels which are needed to query fuzzing
    # coverage APIs.
    gn_path = f'//{rel_path}'
    try:
      reverse_deps = find_reverse_deps(out_dir, gn_path)
      affected = [f for f in reverse_deps if f in all_fuzzers_set]
      return gn_path, affected
    except subprocess.CalledProcessError as e:
      logging.warning(
        f'Failed to find reverse deps for {gn_path}: {e}. '
        'Fuzzing coverage for this file will be missing.'
      )
      return gn_path, []

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    future_to_file = {
      executor.submit(process_file, f): f for f in modified_files
    }
    for future in concurrent.futures.as_completed(future_to_file):
      gn_path, affected = future.result()
      if affected:
        logging.info(f'File "{gn_path}" affects {len(affected)} fuzzers.')
        affected_map[gn_path] = affected
      else:
        logging.debug(f'File "{gn_path}" affects no fuzzers.')
        affected_map[gn_path] = []

  return affected_map


def main():
  parser = argparse.ArgumentParser(
    description='Discover affected fuzzer targets.'
  )
  parser.add_argument(
    '-v', '--verbose', action='store_true', help='Verbose logging.'
  )
  parser.add_argument(
    '--out-dir',
    required=True,
    help='Build directory. GN args will be replaced in this directory.',
  )

  args = parser.parse_args()
  logging.basicConfig(
    level=logging.DEBUG if args.verbose else logging.INFO,
    format='%(levelname)s: %(message)s',
    stream=sys.stderr,
  )

  # Detect modified files
  modified_files = get_modified_files()
  if not modified_files:
    logging.info('No relevant source files modified against HEAD~.')
    return 0

  generate_gn_build_dir(args.out_dir)

  # Get all possible fuzzer targets
  all_fuzzers = find_all_fuzzer_targets(args.out_dir)
  if not all_fuzzers:
    logging.warning(
      'No fuzzer targets found. Ensure "use_libfuzzer = true" in GN args.'
    )
    return 1

  affected_fuzzers_map = get_affected_fuzzers(
    modified_files, args.out_dir, all_fuzzers
  )

  final_output = {'affected_fuzzers': affected_fuzzers_map}
  print(json.dumps(final_output, indent=2, sort_keys=True))
  return 0


if __name__ == '__main__':
  sys.exit(main())
