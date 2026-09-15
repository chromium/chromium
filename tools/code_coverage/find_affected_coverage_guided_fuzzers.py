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
import time
from typing import Dict, List, Optional, Set, Tuple
import urllib.error
import urllib.parse
import urllib.request

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

COVERAGE_API_URL = 'https://analysis.chromium.org/coverage/p/chromium/file'
MAX_FINDIT_API_RETRIES = 3
FINDIT_API_TIMEOUT = 10.0


def generate_gn_build_dir(out_dir: str):
  """Generates the GN graph in the output directory."""
  logging.info('Generating GN graph in %s...', out_dir)

  args_str = gn_args_libfuzzer.replace('\n', ' ')
  command = ['gn', 'gen', out_dir, f'--args={args_str}']

  try:
    subprocess.run(command, cwd=chromium_src_dir, check=True, env=os.environ)
  except subprocess.CalledProcessError as e:
    logging.error('Error running gn gen: %s', e)
    raise


def run_gn_command(args: List[str]) -> List[str]:
  """Runs a gn command and returns stdout lines as a list."""
  try:
    command = ['gn'] + args
    logging.debug('Running command: %s', ' '.join(command))
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
      'Error running gn command: %s\nStdout: %s\nStderr: %s',
      e,
      e.stdout,
      e.stderr,
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
  logging.info('Finding all fuzzer targets in %s...', out_dir)
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
  logging.debug('Finding reverse dependencies for %s...', gn_file_path)
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


def find_affected_fuzzers_for_file(
  file_path: str, out_dir: str, all_fuzzers: Set[str]
) -> Tuple[str, List[str]]:
  """Finds the fuzzers affected by a single modified file."""
  rel_path = file_path.replace(os.sep, '/')
  # Prefix with '//' so the file path is formatted as a GN label, which is what
  # both the reverse-dep query and the fuzzing coverage API expect.
  gn_path = f'//{rel_path}'

  try:
    reverse_deps = find_reverse_deps(out_dir, gn_path)
  except subprocess.CalledProcessError as e:
    logging.warning(
      'Failed to find reverse deps for %s: %s. '
      'Fuzzing coverage for this file will be missing.',
      gn_path,
      e,
    )
    return gn_path, []

  affected_fuzzers = [f for f in reverse_deps if f in all_fuzzers]

  # TODO: Query the Findit fuzzing coverage API for the changed lines of this
  # file and drop candidates with no coverage hits.
  # https://buganizer.corp.google.com/issues/560236801

  # TODO: Include newly added fuzzers as it will not have Findit Fuzzing
  # Coverage API hits.
  # https://buganizer.corp.google.com/issues/561529566

  return gn_path, affected_fuzzers


def get_affected_fuzzers(
  modified_files: List[str], out_dir: str, all_fuzzers: List[str]
) -> Dict[str, List[str]]:
  """Maps modified files to the fuzzers they affect."""
  affected_map: Dict[str, List[str]] = {}
  all_fuzzers_set = set(all_fuzzers)

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = [
      executor.submit(
        find_affected_fuzzers_for_file, f, out_dir, all_fuzzers_set
      )
      for f in modified_files
    ]
    for future in concurrent.futures.as_completed(futures):
      gn_path, affected_fuzzers = future.result()
      logging.info(
        'File "%s" affects %d fuzzers.', gn_path, len(affected_fuzzers)
      )
      affected_map[gn_path] = affected_fuzzers

  return affected_map


def get_findit_coverage(
  file_path: str,
  fuzzer_name: str = 'any',
) -> Optional[List[Dict[str, int]]]:
  """Fetches fuzzing coverage data for a file from the Findit Coverage API.

  Args:
    file_path: Path of the source file
    fuzzer_name: Name of the fuzzer whose coverage is requested, e.g.
      'base_json_reader_fuzzer'. The default, 'any', returns the coverage
      aggregated over all LibFuzzer/FUZZ_TEST fuzzers for the file_path.

  Returns:
    The latest collected fuzzing coverage of the file, as a list of
    {'first': int, 'last': int, 'count': int} objects, where 'first' and
    'last' are the inclusive bounds of a range of consecutive lines that was
    executed 'count' times. Non-executable lines (e.g. comments) are not part
    of any range. Returns None if the coverage data can't be fetched, for
    example because the file or the fuzzer is unknown to the API, or because
    the request keeps failing.

  Raises:
    AssertionError: if file_path is empty.
  """
  assert file_path, 'file_path must be a non-empty path.'

  gn_path = (
    file_path if file_path.startswith('//') else f'//{file_path.lstrip("/")}'
  )

  # Findit coverage API uses 'fuzz' for Libfuzzer and FUZZ_TEST coverage.
  params = urllib.parse.urlencode(
    {
      'path': gn_path,
      'platform': 'fuzz',
      'test_suite_type': fuzzer_name,
      'raw': 'true',
    }
  )
  url = f'{COVERAGE_API_URL}?{params}'

  req = urllib.request.Request(
    url, headers={'User-Agent': 'Chromium-FindAffectedFuzzers/1.0'}
  )

  for attempt in range(MAX_FINDIT_API_RETRIES):
    try:
      with urllib.request.urlopen(req, timeout=FINDIT_API_TIMEOUT) as resp:
        response = json.loads(resp.read().decode('utf-8'))
      return response['data']['metadata']['lines']
    except urllib.error.HTTPError as e:
      # Retry on 429 (rate limit) or 5xx (transient server errors).
      should_retry = e.code in (429, 500, 502, 503, 504)
      error_msg = 'HTTP error %s fetching coverage for %s: %s'
      error_args = (e.code, gn_path, e.reason)
    except (urllib.error.URLError, TimeoutError) as e:
      should_retry = True
      error_msg = 'Network error fetching coverage for %s: %s'
      error_args = (gn_path, e)
    except (json.JSONDecodeError, KeyError) as e:
      # The request succeeded but the response isn't JSON or doesn't have the
      # expected fields, e.g. because the API changed and retrying won't help.
      should_retry = False
      error_msg = 'Malformed coverage response for %s: %r'
      error_args = (gn_path, e)
    except Exception as e:
      should_retry = False
      error_msg = 'Unexpected error fetching coverage for %s: %s'
      error_args = (gn_path, e)

    if should_retry and attempt < MAX_FINDIT_API_RETRIES - 1:
      time.sleep(1.0 * (attempt + 1))
      continue

    logging.warning(error_msg, *error_args)
    break

  return None


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
