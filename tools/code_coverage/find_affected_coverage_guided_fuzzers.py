#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import concurrent.futures
import json
import logging
import os
import re
import subprocess
import sys
import threading
import time
from typing import Dict, List, NamedTuple, Optional, Set, Tuple
import urllib.error
import urllib.parse
import urllib.request

script_dir = os.path.dirname(os.path.realpath(__file__))
chromium_src_dir = os.path.dirname(os.path.dirname(script_dir))

# TODO: Support Centipede fuzzers too.
# https://buganizer.corp.google.com/issues/522382682

COVERAGE_API_URL = 'https://analysis.chromium.org/coverage/p/chromium/file'
MAX_FINDIT_API_RETRIES = 3
FINDIT_API_TIMEOUT = 10.0
# Cap concurrent Findit requests across all files so we don't overload it.
MAX_CONCURRENT_FINDIT_REQUESTS = 32
_findit_request_semaphore = threading.BoundedSemaphore(
  MAX_CONCURRENT_FINDIT_REQUESTS
)

# Matches a unified diff hunk header: '@@ -start,count +start,count @@' for the
# old and new file, with ',count' left out when it's 1, e.g. '@@ -12,3 +14 @@'.
HUNK_HEADER_RE = re.compile(rb'^@@ -(\d+)(?:,(\d+))? \+\d+(?:,(\d+))? @@', re.M)


class ChangedLines(NamedTuple):
  """The lines a CL changes in a file, by their line numbers before the CL."""

  # Lines the CL modifies or deletes.
  modified: Set[int]
  # For each pure insertion, the line it follows or 0 if it's at the top of the
  # file. The inserted lines aren't in any coverage report, so an insertion can
  # only be located by the lines around it.
  insertions: Set[int]


class DiffHunk(NamedTuple):
  """Lines that differ between two versions of a file, as in a unified diff's
  '@@ -old_start,old_count +new_start,new_count @@' header.
  """

  old_start: int
  # 0 for a hunk that only adds lines. old_start is then the line they follow,
  # or 0 if they're at the top of the file.
  old_count: int
  new_count: int


class FinditCoverage(NamedTuple):
  """Fuzzing coverage of a file for one fuzzer or all fuzzers combined."""

  # The commit the report was generated at.
  revision: str
  # Hit count of each line executed at least once, by 1-based line number.
  covered_lines: Dict[int, int]


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


def find_reverse_deps(out_dir: str, gn_file_path: str) -> Set[str]:
  """Finds all targets that depend on the given GN file path."""
  logging.debug('Finding reverse dependencies for %s...', gn_file_path)
  args = ['refs', out_dir, '--all', '--as=label', '-q', gn_file_path]
  return set(run_gn_command(args))


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


def get_diff_hunks(
  cl_revision: str, new_revision: str, file_path: str
) -> List[DiffHunk]:
  """Returns the hunks of file_path's diff from cl_revision's parent to
  new_revision, in file order. Raises if git fails, e.g. on a revision that
  isn't fetched, rather than returning no hunks as if the file were unchanged.
  """
  # git diff-tree, unlike git diff, ignores diff settings from git config that
  # would break the parsing, e.g. color.ui adds color codes to the '@@' lines
  # and diff.interHunkContext merges nearby hunks. With --text, git prints
  # hunks even for a file it thinks is binary, e.g. one with a NUL byte. We
  # don't decode the output since the changed lines may not be valid UTF-8.
  diff = subprocess.run(
    [
      'git',
      'diff-tree',
      '-U0',
      '--text',
      f'{cl_revision}~',
      new_revision,
      '--',
      file_path,
    ],
    cwd=chromium_src_dir,
    capture_output=True,
    check=True,
  ).stdout
  return [
    DiffHunk(
      old_start=int(match[1]),
      old_count=int(match[2] or 1),
      new_count=int(match[3] or 1),
    )
    for match in HUNK_HEADER_RE.finditer(diff)
  ]


def get_parent_lines_changed_by_cl(
  cl_revision: str, file_path: str
) -> ChangedLines:
  """Returns the lines of file_path that cl_revision changes, with the line
  numbers the file had before cl_revision.
  """
  changes = ChangedLines(modified=set(), insertions=set())
  for hunk in get_diff_hunks(cl_revision, cl_revision, file_path):
    if hunk.old_count == 0:
      changes.insertions.add(hunk.old_start)
    else:
      changes.modified.update(
        range(hunk.old_start, hunk.old_start + hunk.old_count)
      )
  return changes


def map_old_line_to_new(hunks: List[DiffHunk], line: int) -> Optional[int]:
  """Finds where a line of a diff's old version is in its new version.

  Args:
    hunks: The diff's hunks from get_diff_hunks(), e.g. from the CL's parent
      to a coverage report's commit.
    line: The line's number in the old version, counting from 1.

  Returns:
    Its number in the new version, or None if the diff modifies or deletes
    it, e.g. because another CL changed it between the two commits.
  """
  offset = 0
  for hunk in hunks:
    if hunk.old_start <= line < hunk.old_start + hunk.old_count:
      return None
    if hunk.old_start >= line:
      # This hunk and the ones after it are below the line.
      break
    # A hunk above the line shifts it by the lines it adds or removes.
    offset += hunk.new_count - hunk.old_count
  return line + offset


def map_changed_lines_to_report_lines(
  changed_lines: ChangedLines,
  cl_revision: str,
  report_revision: str,
  file_path: str,
) -> Set[int]:
  """Maps the CL's changed lines to line numbers in the coverage report.

  Modified lines map directly; pure insertions map to the lines immediately
  before and after them. Lines that differ between the CL's parent and the
  report are skipped.

  Args:
    changed_lines: Changed lines in the CL's parent.
    cl_revision: The CL's commit.
    report_revision: The commit the coverage report was generated at.
    file_path: Repo-relative path of the file.

  Returns:
    1-based line numbers to look up in the coverage report.

  Raises:
    subprocess.CalledProcessError: If git fails (for example, when the
      checkout is missing report_revision).
  """
  # TODO(b/586957271): The report has no coverage data for non-executable lines
  # such as blank lines and comments, so edits to them (e.g. a comment replaced
  # by code) and insertions between them look uncovered. Look those up by the
  # nearest executable lines instead.
  parent_lines = set(changed_lines.modified)
  for after in changed_lines.insertions:
    parent_lines.update((after, after + 1))
  # An insertion at the top of the file follows line 0, which doesn't exist.
  parent_lines.discard(0)
  hunks = get_diff_hunks(cl_revision, report_revision, file_path)
  report_lines = (map_old_line_to_new(hunks, line) for line in parent_lines)
  return {line for line in report_lines if line is not None}


def find_affected_fuzzers_for_file(
  file_path: str, out_dir: str, all_fuzzers: Set[str], revision: str
) -> Tuple[str, Set[str]]:
  """Finds fuzzers whose Findit coverage report covers at least one line
  changed by revision in file_path.

  Starts from reverse-dep fuzzers of file_path. Drops all of them if Findit's
  aggregated report has no data for the file (e.g. a new file) or shows no
  changed line executed; otherwise keeps each fuzzer whose own report at the
  same commit covers a changed line.

  Args:
    file_path: Repo-relative path of the modified source file.
    out_dir: Build directory to query GN reverse dependencies from.
    all_fuzzers: GN labels of all fuzzer executables in out_dir.
    revision: The CL's commit.

  Returns:
    A (gn_path, fuzzers) pair: file_path as a source-absolute path (e.g.
    '//base/foo.cc') and the GN labels of the fuzzers that cover its changed
    lines.

  Raises:
    subprocess.CalledProcessError: If git fails (for example, when the
      checkout is missing the coverage report's commit).
  """
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
    return gn_path, set()

  candidate_fuzzers = reverse_deps & all_fuzzers
  if not candidate_fuzzers:
    return gn_path, set()

  # TODO: Include newly added fuzzers as it will not have Findit Fuzzing
  # Coverage API hits.
  # https://buganizer.corp.google.com/issues/561529566

  changed_lines = get_parent_lines_changed_by_cl(revision, rel_path)

  coverage_report = get_findit_coverage(gn_path)
  if coverage_report is None:
    logging.info('No coverage data for %s, dropping its fuzzers.', gn_path)
    return gn_path, set()

  report_lines = map_changed_lines_to_report_lines(
    changed_lines, revision, coverage_report.revision, rel_path
  )
  logging.debug(
    '%s: looking up lines %s of the coverage report at %s.',
    gn_path,
    sorted(report_lines),
    coverage_report.revision,
  )
  if not any(line in coverage_report.covered_lines for line in report_lines):
    logging.info(
      'No fuzzer covers the lines of %s changed by %s.', gn_path, revision
    )
    return gn_path, set()

  def covers_changed_lines(fuzzer: str) -> bool:
    # Findit looks up coverage by executable name. For a FUZZ_TEST wrapper,
    # that's the GN target name without '__generate' (see //testing/test.gni).
    fuzzer_name = fuzzer.rsplit(':', 1)[-1].removesuffix('__generate')
    # Request the same revision as the aggregated report so report_lines line
    # numbers match.
    fuzzer_coverage_report = get_findit_coverage(
      gn_path, fuzzer_name, coverage_report.revision
    )
    if fuzzer_coverage_report is None:
      return False
    return any(
      line in fuzzer_coverage_report.covered_lines for line in report_lines
    )

  logging.info(
    'Fetching Findit coverage of %s for %d candidate fuzzers...',
    gn_path,
    len(candidate_fuzzers),
  )

  start_time = time.monotonic()
  with concurrent.futures.ThreadPoolExecutor(
    max_workers=MAX_CONCURRENT_FINDIT_REQUESTS
  ) as executor:
    futures = {
      fuzzer: executor.submit(covers_changed_lines, fuzzer)
      for fuzzer in candidate_fuzzers
    }
  covering_fuzzers = {
    fuzzer for fuzzer, future in futures.items() if future.result()
  }

  logging.info(
    'Fetched Findit coverage of %s for %d candidate fuzzers in %.1fs.',
    gn_path,
    len(candidate_fuzzers),
    time.monotonic() - start_time,
  )

  return gn_path, covering_fuzzers


def get_affected_fuzzers(
  modified_files: List[str],
  out_dir: str,
  all_fuzzers: List[str],
  revision: str,
) -> Dict[str, Set[str]]:
  """Maps each modified file to the fuzzers that cover its changed lines."""
  affected_map: Dict[str, Set[str]] = {}
  all_fuzzers_set = set(all_fuzzers)

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = [
      executor.submit(
        find_affected_fuzzers_for_file, f, out_dir, all_fuzzers_set, revision
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


def pick_fuzzers(
  affected_fuzzers_map: Dict[str, Set[str]], max_fuzzers: int
) -> List[str]:
  """Returns the GN labels of the first max_fuzzers relevant fuzzers."""
  # TODO(b/560236802): This is a placeholder. Rank the fuzzers by inverse
  # document frequency (IDF) instead and return the top ones.
  fuzzers: Set[str] = set()
  for fuzzer_file_name in affected_fuzzers_map.values():
    fuzzers.update(fuzzer_file_name)
  picked = sorted(fuzzers)[:max_fuzzers]
  return picked


def get_findit_coverage(
  file_path: str,
  fuzzer_name: str = 'any',
  revision: Optional[str] = None,
) -> Optional[FinditCoverage]:
  """Fetches fuzzing coverage data for a file from the Findit Coverage API.

  Args:
    file_path: Path of the source file.
    fuzzer_name: Fuzzer name (e.g. 'base_json_reader_fuzzer'), or 'any' for
      coverage aggregated across all LibFuzzer/FUZZ_TEST fuzzers.
    revision: Commit of the coverage run to fetch, or None for the latest.

  Returns:
    The file's fuzzing coverage from the requested run. Its covered_lines maps
    only lines executed at least once to their hit counts (unexecuted and
    non-executable lines are omitted, so it is empty when the fuzzer doesn't
    cover the file). None if the coverage can't be fetched, e.g. because
    Findit has no report for the fuzzer, the report doesn't have the file, or
    the request keeps failing.

  Raises:
    AssertionError: if file_path is empty.
  """
  assert file_path, 'file_path must be a non-empty path.'

  gn_path = (
    file_path if file_path.startswith('//') else f'//{file_path.lstrip("/")}'
  )

  # Findit coverage API uses 'fuzz' for Libfuzzer and FUZZ_TEST coverage.
  params_dict = {
    'path': gn_path,
    'platform': 'fuzz',
    'test_suite_type': fuzzer_name,
    'raw': 'true',
  }
  if revision:
    params_dict['revision'] = revision
  params = urllib.parse.urlencode(params_dict)
  url = f'{COVERAGE_API_URL}?{params}'

  req = urllib.request.Request(
    url, headers={'User-Agent': 'Chromium-FindAffectedFuzzers/1.0'}
  )

  for attempt in range(MAX_FINDIT_API_RETRIES):
    log_level = logging.WARNING
    try:
      with _findit_request_semaphore:
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

  'gitiles_commit.revision' is the commit the report was generated at.
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

  covered_lines: Dict[int, int] = {}
  for line_number, line_data in line_to_data:
    # The count is -1 for non-executable lines (e.g. comments) and 0 for
    # executable lines that weren't executed.
    if line_data['count'] > 0:
      covered_lines[line_number] = line_data['count']

  return FinditCoverage(
    revision=response['gitiles_commit']['revision'],
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
    help='Build directory already generated with a fuzzing engine and '
    'sanitizer, e.g. by `tools/mb/mb.py gen -m tryserver.chromium.fuzz '
    '-b linux-x64-libfuzzer-asan-rel-tests <dir>`.',
  )
  parser.add_argument(
    '--revision',
    required=True,
    help='The CL\'s commit. Only files it changes relative to its parent are '
    'considered.',
  )
  parser.add_argument(
    '--max-fuzzers',
    type=int,
    required=True,
    help='Return at most this number of fuzzers to --output-json.',
  )
  parser.add_argument(
    '--output-json',
    required=True,
    help='File to write the GN labels of at most --max-fuzzers '
    'fuzzers to, as {"fuzzers": [label, ...]}. Written whenever the script '
    'exits 0, with {"fuzzers": []} if no fuzzers are found.',
  )

  args = parser.parse_args()
  if args.max_fuzzers < 1:
    parser.error('--max-fuzzers must be at least 1.')

  # The fuzz_cq recipe generates --out-dir, passes it to this script and then
  # builds and runs the fuzzers in it. The recipe picks the GN args because they
  # set the fuzzing engine and sanitizer that the fuzzers are built and run
  # with. The GN args also decide which fuzzers exist and which files they
  # depend on, so this script runs gn refs on the same --out-dir to find the
  # relevant fuzzers.
  args_gn = os.path.join(chromium_src_dir, args.out_dir, 'args.gn')
  if not os.path.isfile(args_gn):
    parser.error(f'{args.out_dir} has no args.gn. Generate it first.')
  logging.basicConfig(
    level=logging.DEBUG if args.verbose else logging.INFO,
    format='%(levelname)s: %(message)s',
    stream=sys.stderr,
  )

  # Detect modified files
  modified_files = get_modified_files(args.revision)
  if not modified_files:
    logging.info('No relevant source files modified by %s.', args.revision)
    with open(args.output_json, 'w') as f:
      json.dump({'fuzzers': []}, f, indent=2)
    return 0

  # Get all possible fuzzer targets
  all_fuzzers = find_all_fuzzer_targets(args.out_dir)
  if not all_fuzzers:
    logging.warning(
      'No fuzzer targets found in %s. Was it generated with a fuzzing engine, '
      'e.g. use_libfuzzer = true?',
      args.out_dir,
    )
    return 1

  affected_fuzzers_map = get_affected_fuzzers(
    modified_files, args.out_dir, all_fuzzers, args.revision
  )

  fuzzers = pick_fuzzers(affected_fuzzers_map, args.max_fuzzers)
  with open(args.output_json, 'w') as f:
    json.dump({'fuzzers': fuzzers}, f, indent=2)
  return 0


if __name__ == '__main__':
  sys.exit(main())
