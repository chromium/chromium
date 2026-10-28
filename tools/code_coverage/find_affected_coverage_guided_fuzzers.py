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
from typing import Dict, List, NamedTuple, Optional, Set, Tuple
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
use_remoteexec = true
use_siso = true
"""

# TODO: Support Centipede fuzzers too.
# https://buganizer.corp.google.com/issues/522382682

COVERAGE_API_URL = 'https://analysis.chromium.org/coverage/p/chromium/file'
MAX_FINDIT_API_RETRIES = 3
FINDIT_API_TIMEOUT = 10.0


class FinditCoverage(NamedTuple):
  """Fuzzing coverage of a file for one fuzzer or all fuzzers combined."""

  # The file's lines in the report, in file order: file_lines[i] is line i + 1.
  file_lines: List[str]
  # Hit count of each line executed at least once, by 1-based line number.
  covered_lines: Dict[int, int]


def generate_gn_build_dir(out_dir: str):
  """Generates the GN graph in the output directory."""
  logging.info('Generating GN graph in %s...', out_dir)

  args_str = gn_args_libfuzzer.replace('\n', ' ')
  command = ['gn', 'gen', out_dir, f'--args={args_str}']

  try:
    # Redirect gn gen's output to stderr so it doesn't get mixed into the
    # JSON result this script prints to stdout.
    subprocess.run(
      command,
      cwd=chromium_src_dir,
      check=True,
      env=os.environ,
      stdout=sys.stderr,
    )
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


def get_modified_files(revision: str) -> List[str]:
  """Detects C/C++ files modified by the given commit."""
  # Diff against the commit's own parent rather than HEAD~: bot_update
  # soft-resets the CL, with any unlanded CLs it's stacked on, onto a main
  # commit, so HEAD~ would also pick up those CLs and the last main commit.
  logging.info('Detecting files modified by %s...', revision)
  command = [
    'git',
    'diff',
    '--name-only',
    '--diff-filter=d',
    '-z',
    f'{revision}~',
    revision,
  ]
  result = subprocess.run(
    command, cwd=chromium_src_dir, capture_output=True, text=True, check=True
  )
  return [
    f
    for f in result.stdout.split('\0')
    if f.endswith(('.cc', '.cpp', '.c', '.h', '.m', '.mm'))
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

  # TODO: Before matching, map the changed lines from the CL's parent commit
  # to the coverage report's commit, since the file may have changed in
  # between. The Findit response has the report's version of the file in
  # data.line_to_data to diff the parent's version against.
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
) -> Optional[FinditCoverage]:
  """Fetches fuzzing coverage data for a file from the Findit Coverage API.

  Args:
    file_path: Path of the source file
    fuzzer_name: Name of the fuzzer whose coverage is requested, e.g.
      'base_json_reader_fuzzer'. The default, 'any', returns the coverage
      aggregated over all LibFuzzer/FUZZ_TEST fuzzers for the file_path.

  Returns:
    The latest collected fuzzing coverage of the file. Its covered_lines is
    empty if the coverage was fetched but the fuzzer doesn't cover the file.
    None if the coverage can't be fetched, e.g. because Findit has no report
    for the fuzzer, the report doesn't have the file, or the request keeps
    failing.

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
    log_level = logging.WARNING
    try:
      with urllib.request.urlopen(req, timeout=FINDIT_API_TIMEOUT) as resp:
        response = json.loads(resp.read().decode('utf-8'))
      return parse_findit_response(response, gn_path)
    except urllib.error.HTTPError as e:
      # Retry on 429 (rate limit) and on 500, 502, 503 and 504 (transient
      # server errors), except on a 500 for a single fuzzer: Findit answers
      # 500 when it has no report for the fuzzer, which retrying won't fix.
      # TODO(b/571233495): Retry those 500s too once Findit is redeployed and
      # returns 404 for a missing report instead.
      no_report = e.code == 500 and fuzzer_name != 'any'
      should_retry = e.code in (429, 500, 502, 503, 504) and not no_report
      if no_report:
        # Fuzzers sometimes have no reports e.g. for newly added fuzzers
        # or fuzzers tagged no_clusterfuzz.
        log_level = logging.INFO
      error_msg = 'HTTP error %s fetching %s coverage for %s: %s'
      error_args = (e.code, fuzzer_name, gn_path, e.reason)
    except (urllib.error.URLError, TimeoutError) as e:
      should_retry = True
      error_msg = 'Network error fetching coverage for %s: %s'
      error_args = (gn_path, e)
    except (json.JSONDecodeError, KeyError, TypeError) as e:
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

    logging.log(log_level, error_msg, *error_args)
    break

  return None


def parse_findit_response(
  response: Dict, file_path: str
) -> Optional[FinditCoverage]:
  """Extracts the coverage of a file from a raw Findit Coverage API response.

  'data.line_to_data' is a list of [line_number, {'line': str, 'count': int,
  ...}] pairs with each line of the file at the report's revision and its hit
  count.

  Args:
    response: The response to a request for the coverage of file_path.
    file_path: Source-absolute path of the file, e.g.
      '//base/json/json_reader.cc'.

  Returns:
    The coverage or None. If the report doesn't contain the file, Findit
    returns the summary of the root directory, and we return None in this
    case. We also return None if Findit couldn't load the file's content.

  Raises:
    KeyError, TypeError: if the response doesn't have the expected fields.
  """
  if response['path'] != file_path:
    # TODO(crbug.com/578475336): This is a temporary check. We should expect a
    # 404 once Findit returns 404 for files that aren't in the report.
    logging.debug('No coverage for %s: %s', file_path, response['warning'])
    return None

  line_to_data = response['data']['line_to_data']
  if line_to_data is None:
    # Findit couldn't load the file's content from GCS or Gitiles.
    # TODO(crbug.com/578476159): Expect an error instead once Findit returns an
    # error code instead of None line_to_data.
    logging.warning('Findit has no content for %s', file_path)
    return None

  file_lines: List[str] = []
  covered_lines: Dict[int, int] = {}
  for line_number, line_data in line_to_data:
    file_lines.append(line_data['line'])
    # The count is -1 for non-executable lines (e.g. comments) and 0 for
    # executable lines that weren't executed.
    if line_data['count'] > 0:
      covered_lines[line_number] = line_data['count']

  return FinditCoverage(
    file_lines=file_lines,
    covered_lines=covered_lines,
  )


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
  parser.add_argument(
    '--revision',
    required=True,
    help='The CL\'s commit. Only files it changes relative to its parent are '
    'considered.',
  )

  args = parser.parse_args()
  logging.basicConfig(
    level=logging.DEBUG if args.verbose else logging.INFO,
    format='%(levelname)s: %(message)s',
    stream=sys.stderr,
  )

  # Detect modified files
  modified_files = get_modified_files(args.revision)
  if not modified_files:
    logging.info('No relevant source files modified by %s.', args.revision)
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
