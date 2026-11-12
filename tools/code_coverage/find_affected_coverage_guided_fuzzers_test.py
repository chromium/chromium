#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import logging
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import urllib.error
from urllib.parse import parse_qs
from urllib.parse import urlparse

# Add the directory containing the script to the path so we can import it
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import find_affected_coverage_guided_fuzzers

# make_findit_response()'s covered lines, with their hit counts.
FINDIT_COVERED_LINES = {2: 7, 3: 7, 5: 7}
# The commit of make_findit_response()'s report.
REPORT_REVISION = 'a' * 40


def output_json_path(out_dir):
  # The out dir is a temporary directory, so write the output there too.
  return os.path.join(out_dir, 'fuzzers.json')


def main_argv(out_dir, max_fuzzers=5):
  return [
    'script.py',
    '--out-dir',
    out_dir,
    '--revision',
    'abc',
    '--max-fuzzers',
    str(max_fuzzers),
    '--output-json',
    output_json_path(out_dir),
  ]


def read_output_json(out_dir):
  return json.loads(pathlib.Path(output_json_path(out_dir)).read_text())


def make_findit_response(covered=True):
  count = 7 if covered else 0
  return {
    'data': {
      'line_to_data': [
        [1, {'line': '// Comment.', 'count': -1}],
        [2, {'line': 'void F() {', 'count': count}],
        [3, {'line': '  if (G())', 'count': count}],
        [4, {'line': '    H();', 'count': 0}],
        [5, {'line': '}', 'count': count}],
      ],
    },
    'gitiles_commit': {'revision': REPORT_REVISION},
    'path': '//base/test.cc',
    'warning': None,
  }


class FindAffectedCoverageGuidedFuzzersTest(unittest.TestCase):
  def setUp(self):
    patcher = mock.patch('find_affected_coverage_guided_fuzzers.subprocess.run')
    self.mock_subprocess_run = patcher.start()
    self.addCleanup(patcher.stop)

    # Several tests exercise error paths that log warnings/errors on purpose.
    # Without any handler on the root logger, Python falls back to its
    # "last resort" handler, which prints those messages to stderr and makes
    # the test output look like something went wrong. A NullHandler keeps the
    # messages discarded while still letting assertLogs() capture them.
    null_handler = logging.NullHandler()
    logging.root.addHandler(null_handler)
    self.addCleanup(logging.root.removeHandler, null_handler)

  def get_calls_args(self, mock_obj):
    return [' '.join(call.args[0]) for call in mock_obj.call_args_list]

  def _set_subprocess_run_side_effect(
    self, git_diff_stdout='', gn_refs_mapping=None
  ):

    def fake_subprocess_run(command, **kwargs):
      mock_result = mock.MagicMock()
      mock_result.stdout = ''
      if command[0] == 'git':
        mock_result.stdout = git_diff_stdout
      elif command[:2] == ['gn', 'refs']:
        if gn_refs_mapping:
          for k, v in gn_refs_mapping.items():
            if k in command:
              mock_result.stdout = v
              break
      return mock_result

    self.mock_subprocess_run.side_effect = fake_subprocess_run

  def make_out_dir(self):
    """Returns a temporary build directory with an empty args.gn."""
    out_dir = self.enterContext(tempfile.TemporaryDirectory())
    pathlib.Path(out_dir, 'args.gn').touch()
    return out_dir

  def test_get_modified_files(self):
    self.mock_subprocess_run.return_value.stdout = (
      'file1.cc\0file2.h\0file3.txt\0'
    )
    files = find_affected_coverage_guided_fuzzers.get_modified_files('abc')
    self.assertEqual(files, ['file1.cc', 'file2.h'])
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      ['git diff --name-only --diff-filter=d -z abc~ abc'],
    )

  def test_parse_findit_response(self):
    coverage = find_affected_coverage_guided_fuzzers.parse_findit_response(
      make_findit_response(), '//base/test.cc'
    )
    self.assertEqual(coverage.revision, REPORT_REVISION)
    self.assertEqual(coverage.covered_lines, FINDIT_COVERED_LINES)

  def test_parse_findit_response_without_covered_lines(self):
    coverage = find_affected_coverage_guided_fuzzers.parse_findit_response(
      make_findit_response(covered=False), '//base/test.cc'
    )
    self.assertEqual(coverage.revision, REPORT_REVISION)
    self.assertEqual(coverage.covered_lines, {})

  def test_parse_findit_response_without_file_content(self):
    # Findit answers with a null line_to_data when it can't load the file's
    # content.
    # TODO(crbug.com/578476159): Expect an error instead once Findit returns an
    # HTTP error code instead of null line_to_data.
    response = make_findit_response()
    response['data']['line_to_data'] = None
    coverage = find_affected_coverage_guided_fuzzers.parse_findit_response(
      response, '//base/test.cc'
    )
    self.assertIsNone(coverage)

  def test_parse_findit_response_for_file_not_in_report(self):
    # Findit answers with the summary of the root directory when the file isn't
    # in the report.
    # TODO(crbug.com/578475336): Remove this test with the path check once
    # Findit answers 404 instead.
    response = {
      'data': {'metadata': {'path': '//', 'dirs': [], 'summaries': []}},
      'data_type': 'dirs',
      'path': '//',
      'warning': 'File "//base/unknown_file.cc" does not exist in this '
      'report, defaulting to root',
    }
    coverage = find_affected_coverage_guided_fuzzers.parse_findit_response(
      response, '//base/unknown_file.cc'
    )
    self.assertIsNone(coverage)

  def test_get_affected_fuzzers(self):
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={
        '//file1.cc': '//fuzzer1:fuzzer1\n//fuzzer2:fuzzer2\n',
        '//file2.cc': '//fuzzer2:fuzzer2\n',
      }
    )

    modified_files = ['file1.cc', 'file2.cc', 'file3.cc']
    all_fuzzers = [
      '//fuzzer1:fuzzer1',
      '//fuzzer2:fuzzer2',
      '//fuzzer3:fuzzer3',
    ]

    affected = find_affected_coverage_guided_fuzzers.get_affected_fuzzers(
      modified_files, 'out/test', all_fuzzers
    )

    # '//file3.cc' does not affect any fuzzer, so it maps to an empty list.
    self.assertEqual(
      affected,
      {
        '//file1.cc': ['//fuzzer1:fuzzer1', '//fuzzer2:fuzzer2'],
        '//file2.cc': ['//fuzzer2:fuzzer2'],
        '//file3.cc': [],
      },
    )

    # We expect 3 calls to gn refs, one for each modified file.
    self.assertEqual(self.mock_subprocess_run.call_count, 3)
    # The order is non-deterministic because it uses a ThreadPoolExecutor,
    # so we just check that the calls happened with the right arguments.
    call_args_files = {
      call.split()[-1] for call in self.get_calls_args(self.mock_subprocess_run)
    }
    self.assertEqual(
      call_args_files, {'//file1.cc', '//file2.cc', '//file3.cc'}
    )

  def test_get_affected_fuzzers_with_exception(self):

    def fake_subprocess_run(command, **kwargs):
      mock_result = mock.MagicMock()
      mock_result.stdout = ''
      if command[:2] == ['gn', 'refs']:
        if '//file1.cc' in command:
          mock_result.stdout = '//fuzzer1:fuzzer1\n'
        elif '//file2.cc' in command:
          subprocess_mod = find_affected_coverage_guided_fuzzers.subprocess
          raise subprocess_mod.CalledProcessError(
            returncode=1, cmd=command, output='out', stderr='err'
          )
      return mock_result

    self.mock_subprocess_run.side_effect = fake_subprocess_run

    modified_files = ['file1.cc', 'file2.cc']
    all_fuzzers = ['//fuzzer1:fuzzer1']

    affected = find_affected_coverage_guided_fuzzers.get_affected_fuzzers(
      modified_files, 'out/test', all_fuzzers
    )

    # The reverse deps of '//file2.cc' can't be computed, so no fuzzer is
    # reported for it.
    self.assertEqual(
      affected, {'//file1.cc': ['//fuzzer1:fuzzer1'], '//file2.cc': []}
    )

  def test_run_gn_command_success(self):
    self.mock_subprocess_run.return_value.stdout = (
      'target1\ntarget2\n\ntarget1\n'
    )
    result = find_affected_coverage_guided_fuzzers.run_gn_command(
      ['refs', 'out/test']
    )
    self.assertEqual(result, ['target1', 'target2', 'target1'])
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run), ['gn refs out/test']
    )

  def test_run_gn_command_called_process_error(self):
    self.mock_subprocess_run.side_effect = (
      find_affected_coverage_guided_fuzzers.subprocess.CalledProcessError(
        returncode=1, cmd=['gn'], output='out', stderr='err'
      )
    )
    with self.assertLogs(level='ERROR') as cm:
      with self.assertRaises(
        find_affected_coverage_guided_fuzzers.subprocess.CalledProcessError
      ):
        find_affected_coverage_guided_fuzzers.run_gn_command(
          ['refs', 'out/test']
        )
    self.assertIn('Error running gn command', cm.output[0])

  def test_run_gn_command_file_not_found_error(self):
    self.mock_subprocess_run.side_effect = FileNotFoundError()
    with self.assertLogs(level='ERROR') as cm:
      with self.assertRaises(FileNotFoundError):
        find_affected_coverage_guided_fuzzers.run_gn_command(
          ['refs', 'out/test']
        )
    self.assertIn("'gn' command not found", cm.output[0])

  def test_find_all_fuzzer_targets(self):
    self.mock_subprocess_run.return_value.stdout = '//fuzzer1:fuzzer1\n'
    result = find_affected_coverage_guided_fuzzers.find_all_fuzzer_targets(
      'out/test'
    )
    self.assertEqual(result, ['//fuzzer1:fuzzer1'])
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      [
        'gn refs out/test //testing/libfuzzer:fuzzing_engine '
        '--all --type=executable --as=label -q'
      ],
    )

  def test_find_reverse_deps(self):
    self.mock_subprocess_run.return_value.stdout = '//target1:target1\n'
    result = find_affected_coverage_guided_fuzzers.find_reverse_deps(
      'out/test', '//file1.cc'
    )
    self.assertEqual(result, ['//target1:target1'])
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      ['gn refs out/test --all --as=label -q //file1.cc'],
    )

  def test_main_out_dir_not_generated(self):
    out_dir = self.enterContext(tempfile.TemporaryDirectory())
    with mock.patch('sys.argv', main_argv(out_dir)):
      with mock.patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
        with self.assertRaises(SystemExit) as cm:
          find_affected_coverage_guided_fuzzers.main()
    self.assertEqual(cm.exception.code, 2)
    self.assertIn('has no args.gn. Generate it first.', mock_stderr.getvalue())
    self.mock_subprocess_run.assert_not_called()

  def test_main_max_fuzzers_must_be_positive(self):
    with mock.patch('sys.argv', main_argv(self.make_out_dir(), max_fuzzers=0)):
      with mock.patch('sys.stderr', new_callable=io.StringIO) as mock_stderr:
        with self.assertRaises(SystemExit) as cm:
          find_affected_coverage_guided_fuzzers.main()
    self.assertEqual(cm.exception.code, 2)
    self.assertIn('--max-fuzzers must be at least 1.', mock_stderr.getvalue())
    self.mock_subprocess_run.assert_not_called()

  def test_main_no_modified_files(self):
    self.mock_subprocess_run.return_value.stdout = ''
    out_dir = self.make_out_dir()
    with mock.patch('sys.argv', main_argv(out_dir)):
      self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 0)
    self.assertEqual(read_output_json(out_dir), {'fuzzers': []})
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      ['git diff --name-only --diff-filter=d -z abc~ abc'],
    )

  def test_main_no_fuzzers(self):
    self._set_subprocess_run_side_effect(git_diff_stdout='file1.cc\0')
    out_dir = self.make_out_dir()

    with mock.patch('sys.argv', main_argv(out_dir)):
      with self.assertLogs(level='WARNING') as cm:
        self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 1)
    self.assertIn('No fuzzer targets found', cm.output[0])

    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      [
        'git diff --name-only --diff-filter=d -z abc~ abc',
        f'gn refs {out_dir} //testing/libfuzzer:fuzzing_engine '
        '--all --type=executable --as=label -q',
      ],
    )

  def test_main_success(self):
    self._set_subprocess_run_side_effect(
      git_diff_stdout='file1.cc\0',
      gn_refs_mapping={
        '//testing/libfuzzer:fuzzing_engine': '//fuzzer:fuzzer\n',
        '//file1.cc': '//fuzzer:fuzzer\n',
      },
    )
    out_dir = self.make_out_dir()

    with mock.patch('sys.argv', main_argv(out_dir)):
      self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 0)
    self.assertEqual(
      read_output_json(out_dir), {'fuzzers': ['//fuzzer:fuzzer']}
    )

    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      [
        'git diff --name-only --diff-filter=d -z abc~ abc',
        f'gn refs {out_dir} //testing/libfuzzer:fuzzing_engine '
        '--all --type=executable --as=label -q',
        f'gn refs {out_dir} --all --as=label -q //file1.cc',
      ],
    )

  def test_main_returns_at_most_max_fuzzers(self):
    self._set_subprocess_run_side_effect(
      git_diff_stdout='file1.cc\0',
      gn_refs_mapping={
        '//testing/libfuzzer:fuzzing_engine': (
          '//fuzzer1:fuzzer1\n//fuzzer2:fuzzer2\n'
        ),
        '//file1.cc': '//fuzzer1:fuzzer1\n//fuzzer2:fuzzer2\n',
      },
    )
    out_dir = self.make_out_dir()

    with mock.patch('sys.argv', main_argv(out_dir, max_fuzzers=1)):
      self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 0)
    self.assertEqual(
      read_output_json(out_dir), {'fuzzers': ['//fuzzer1:fuzzer1']}
    )

  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_success(self, mock_urlopen):
    fake_response = mock.MagicMock()
    fake_response.read.return_value = json.dumps(make_findit_response()).encode(
      'utf-8'
    )
    mock_urlopen.return_value.__enter__.return_value = fake_response

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      'base/test.cc', 'base_json_reader_fuzzer'
    )
    self.assertEqual(result.covered_lines, FINDIT_COVERED_LINES)
    self.assertEqual(result.revision, REPORT_REVISION)

    req = mock_urlopen.call_args[0][0]
    self.assertEqual(
      parse_qs(urlparse(req.full_url).query),
      {
        'path': ['//base/test.cc'],
        'platform': ['fuzz'],
        'test_suite_type': ['base_json_reader_fuzzer'],
        'raw': ['true'],
      },
    )

  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_defaults_to_aggregated_coverage(
    self, mock_urlopen
  ):
    fake_response = mock.MagicMock()
    fake_response.read.return_value = json.dumps(make_findit_response()).encode(
      'utf-8'
    )
    mock_urlopen.return_value.__enter__.return_value = fake_response

    find_affected_coverage_guided_fuzzers.get_findit_coverage('//base/test.cc')

    req = mock_urlopen.call_args[0][0]
    self.assertEqual(
      parse_qs(urlparse(req.full_url).query),
      {
        'path': ['//base/test.cc'],
        'platform': ['fuzz'],
        # 'any' is Findit's name for the coverage aggregated over all fuzzers.
        'test_suite_type': ['any'],
        'raw': ['true'],
      },
    )

  @mock.patch('find_affected_coverage_guided_fuzzers.time.sleep')
  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_with_unexpected_coverage_data(
    self, mock_urlopen, mock_sleep
  ):
    fake_response = mock.MagicMock()
    fake_response.read.return_value = json.dumps({'data': {}}).encode('utf-8')
    mock_urlopen.return_value.__enter__.return_value = fake_response

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/test.cc'
    )

    # The response doesn't have the expected format, so no coverage is
    # reported for the file. Confirm after the API returns malformed data
    # that there are no attempted retries.
    self.assertIsNone(result)
    self.assertEqual(mock_urlopen.call_count, 1)
    mock_sleep.assert_not_called()

  def test_get_findit_coverage_with_empty_path(self):
    with self.assertRaises(AssertionError):
      find_affected_coverage_guided_fuzzers.get_findit_coverage('')

  @mock.patch('find_affected_coverage_guided_fuzzers.time.sleep')
  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_retries_on_server_error(
    self, mock_urlopen, mock_sleep
  ):
    fake_response = mock.MagicMock()
    fake_response.read.return_value = json.dumps(make_findit_response()).encode(
      'utf-8'
    )
    mock_urlopen.side_effect = [
      urllib.error.HTTPError(
        'http://example.com', 503, 'Service Unavailable', {}, None
      ),
      mock.MagicMock(__enter__=mock.MagicMock(return_value=fake_response)),
    ]

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/test.cc', 'base_json_reader_fuzzer'
    )

    self.assertEqual(result.covered_lines, FINDIT_COVERED_LINES)
    self.assertEqual(mock_urlopen.call_count, 2)
    self.assertEqual(mock_sleep.call_count, 1)

  @mock.patch('find_affected_coverage_guided_fuzzers.time.sleep')
  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_retries_500_for_aggregated_coverage(
    self, mock_urlopen, mock_sleep
  ):
    mock_urlopen.side_effect = urllib.error.HTTPError(
      'http://example.com', 500, 'Server Error', {}, None
    )

    # Unlike a single fuzzer, the aggregated report exists as long as the
    # fuzz coverage builder has run so a 500 for it is a real failure.
    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/test.cc'
    )

    max_retries = find_affected_coverage_guided_fuzzers.MAX_FINDIT_API_RETRIES
    self.assertIsNone(result)
    self.assertEqual(mock_urlopen.call_count, max_retries)
    self.assertEqual(mock_sleep.call_count, max_retries - 1)

  @mock.patch('find_affected_coverage_guided_fuzzers.time.sleep')
  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_does_not_retry_500_for_fuzzer(
    self, mock_urlopen, mock_sleep
  ):
    # Findit answers 500 when it has no coverage report for the fuzzer.
    # TODO(b/571233495): Expect retries once Findit is redeployed and answers
    # 404 instead.
    mock_urlopen.side_effect = urllib.error.HTTPError(
      'http://example.com', 500, 'Server Error', {}, None
    )

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/test.cc', 'unknown_fuzzer'
    )

    self.assertIsNone(result)
    self.assertEqual(mock_urlopen.call_count, 1)
    mock_sleep.assert_not_called()

  @mock.patch('find_affected_coverage_guided_fuzzers.time.sleep')
  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_does_not_retry_on_client_error(
    self, mock_urlopen, mock_sleep
  ):
    mock_urlopen.side_effect = urllib.error.HTTPError(
      'http://example.com', 404, 'Not Found', {}, None
    )

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/notfound.cc'
    )

    self.assertIsNone(result)
    self.assertEqual(mock_urlopen.call_count, 1)
    mock_sleep.assert_not_called()


class GitHelpersTest(unittest.TestCase):
  """Runs the git helpers on a real repo, as they rely on how git behaves."""

  def setUp(self):
    self.repo = self.enterContext(tempfile.TemporaryDirectory())
    self.enterContext(
      mock.patch.object(
        find_affected_coverage_guided_fuzzers, 'chromium_src_dir', self.repo
      )
    )
    # Keep the user's git config and GIT_* variables (e.g. GIT_DIR when run
    # from a git hook) out of the test.
    env = {k: v for k, v in os.environ.items() if not k.startswith('GIT_')}
    env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')
    self.enterContext(mock.patch.dict(os.environ, env, clear=True))
    self.git('init', '-q')
    self.git('config', 'user.name', 'Test')
    self.git('config', 'user.email', 'test@example.com')

  def git(self, *args: str) -> str:
    return subprocess.run(
      ['git', *args], cwd=self.repo, capture_output=True, text=True, check=True
    ).stdout.strip()

  def commit(self, path: str, content: str) -> str:
    """Commits content as path and returns the commit's hash."""
    full_path = os.path.join(self.repo, path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, 'w', encoding='utf-8', newline='') as f:
      f.write(content)
    self.git('add', path)
    self.git('commit', '-q', '-m', path)
    return self.git('rev-parse', 'HEAD')

  def test_get_parent_lines_changed_by_cl(self):
    # Check that a developer's or bot's git config can't break get_diff_hunks().
    # These settings break `git diff`:
    self.git('config', 'color.ui', 'always')  # Colors the '@@' lines.
    self.git('config', 'diff.external', 'true')  # Prints nothing.
    self.git('config', 'diff.interHunkContext', '10')  # Merges nearby hunks.
    self.commit('base/f.cc', ''.join(f'{i}\n' for i in range(1, 21)))
    revision = self.commit(
      'base/f.cc',
      'top\n'  # Inserted at the top.
      '1\n2\n'
      'three\n'  # Line 3 modified.
      '4\n5\n'
      'a\nb\nc\n'  # Lines 6-7 replaced by three lines.
      '8\n9\n10\n'
      'x\ny\n'  # Two lines inserted after line 10.
      '11\n12\n13\n14\n'  # Lines 15-17 deleted.
      '18\n19\n20\n',
    )
    self.commit('base/g.cc', 'later\n')  # So that revision isn't HEAD.
    # A pure insertion is returned as the line it follows.
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.get_parent_lines_changed_by_cl(
        revision, 'base/f.cc'
      ),
      find_affected_coverage_guided_fuzzers.ChangedLines(
        modified={3, 6, 7, 15, 16, 17}, insertions={0, 10}
      ),
    )

  def test_get_parent_lines_changed_by_cl_new_file(self):
    self.commit('base/f.cc', 'a\n')  # So that revision has a parent.
    revision = self.commit('base/new.cc', 'b\nc\n')
    # All of a new file's lines are one insertion at the top.
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.get_parent_lines_changed_by_cl(
        revision, 'base/new.cc'
      ),
      find_affected_coverage_guided_fuzzers.ChangedLines(
        modified=set(), insertions={0}
      ),
    )

  def test_map_old_line_to_new(self):
    # Between the report's commit and the CL's parent, another CL added x and
    # y, and removed F.
    report = self.commit('base/f.cc', 'A\nB\nC\nD\nE\nF\nG\nH\n')
    parent = self.commit('base/f.cc', 'A\nB\nx\ny\nC\nD\nE\nG\nH\n')
    hunks = find_affected_coverage_guided_fuzzers.get_diff_hunks(
      parent, report, 'base/f.cc'
    )
    self.assertEqual(
      [
        find_affected_coverage_guided_fuzzers.map_old_line_to_new(hunks, line)
        for line in range(1, 10)
      ],
      [1, 2, None, None, 3, 4, 5, 7, 8],
    )

  def test_get_diff_hunks_binary_looking_file(self):
    # Without --text, git prints no hunks for a file with a NUL byte, so the
    # file looks unchanged.
    old = self.commit('base/f.cc', 'a\0\nb\n')
    new = self.commit('base/f.cc', 'a\0\nB\n')
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.get_diff_hunks(
        old, new, 'base/f.cc'
      ),
      [
        find_affected_coverage_guided_fuzzers.DiffHunk(
          old_start=2, old_count=1, new_count=1
        )
      ],
    )

  def test_get_diff_hunks_missing_revision(self):
    # E.g. a coverage report's commit that the checkout hasn't fetched.
    revision = self.commit('base/f.cc', 'a\n')
    with self.assertRaises(subprocess.CalledProcessError):
      find_affected_coverage_guided_fuzzers.get_diff_hunks(
        '1234567890abcdef1234567890abcdef12345678', revision, 'base/f.cc'
      )


if __name__ == '__main__':
  unittest.main()
