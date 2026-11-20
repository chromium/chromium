#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import concurrent.futures
import io
import json
import logging
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
import time
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
# An older report commit, for testing fuzzers absent from the latest run.
OLD_REPORT_REVISION = 'b' * 40

# Mock `git diff-tree` output keyed by the target revision. CL 'abc' changes
# lines 1-3; REPORT_REVISION matches the CL's parent by default.
DIFF_TREE_STDOUT = {
  'abc': b'@@ -1,3 +1,3 @@\n',
  REPORT_REVISION: b'',
}


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


def make_findit_response(covered=True, revision=REPORT_REVISION):
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
    'gitiles_commit': {'revision': revision},
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
    self,
    git_diff_stdout='',
    gn_refs_mapping=None,
    diff_tree_stdout=DIFF_TREE_STDOUT,
  ):

    def fake_subprocess_run(command, **kwargs):
      mock_result = mock.MagicMock()
      mock_result.stdout = ''
      if command[:2] == ['git', 'diff-tree']:
        # `git diff-tree` takes `<old> <new> -- <path>`.
        new_revision = command[command.index('--') - 1]
        if new_revision not in diff_tree_stdout:
          # Simulate git failing on a commit missing from the local checkout.
          raise subprocess.CalledProcessError(128, command)
        mock_result.stdout = diff_tree_stdout[new_revision]
      elif command[0] == 'git':
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

  def _mock_findit(self, coverage_by_fuzzer):
    """Patches get_findit_coverage() using coverage_by_fuzzer, a map from
    test_suite_type ('any' or a fuzzer name) to its raw API response or None.
    Returns the list of (file_path, fuzzer_name, revision) queried.
    """
    module = find_affected_coverage_guided_fuzzers
    queries = []

    def fake_get_findit_coverage(file_path, fuzzer_name='any', revision=None):
      queries.append((file_path, fuzzer_name, revision))
      response = coverage_by_fuzzer.get(fuzzer_name)
      if not response:
        return None
      if revision and revision != response['gitiles_commit']['revision']:
        return None
      return module.parse_findit_response(
        dict(response, path=file_path), file_path
      )

    patcher = mock.patch.object(
      module, 'get_findit_coverage', side_effect=fake_get_findit_coverage
    )
    self.addCleanup(patcher.stop)
    patcher.start()
    return queries

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
        # Not every target that depends on a file is a fuzzer.
        '//file1.cc': '//base:base\n//fuzzer1:fuzzer1\n//fuzzer2:fuzzer2\n',
        '//file2.cc': '//fuzzer2:fuzzer2\n',
      }
    )
    findit_queries = self._mock_findit(
      {
        'any': make_findit_response(),
        'fuzzer1': make_findit_response(),
        'fuzzer2': make_findit_response(),
      }
    )

    modified_files = ['file1.cc', 'file2.cc', 'file3.cc']
    all_fuzzers = [
      '//fuzzer1:fuzzer1',
      '//fuzzer2:fuzzer2',
      '//fuzzer3:fuzzer3',
    ]

    affected = find_affected_coverage_guided_fuzzers.get_affected_fuzzers(
      modified_files, 'out/test', all_fuzzers, 'abc'
    )

    # '//file3.cc' does not affect any fuzzer, so it maps to an empty set.
    self.assertEqual(
      affected,
      {
        '//file1.cc': {'//fuzzer1:fuzzer1', '//fuzzer2:fuzzer2'},
        '//file2.cc': {'//fuzzer2:fuzzer2'},
        '//file3.cc': set(),
      },
    )

    # The order is non-deterministic because it uses a ThreadPoolExecutor,
    # so we just check that the calls happened with the right arguments.
    gn_refs_files = {
      call.split()[-1]
      for call in self.get_calls_args(self.mock_subprocess_run)
      if call.startswith('gn refs')
    }
    self.assertEqual(gn_refs_files, {'//file1.cc', '//file2.cc', '//file3.cc'})
    # '//file3.cc' has no candidate fuzzers, so no coverage is fetched for it.
    self.assertCountEqual(
      findit_queries,
      [
        ('//file1.cc', 'any', None),
        ('//file1.cc', 'fuzzer1', REPORT_REVISION),
        ('//file1.cc', 'fuzzer2', REPORT_REVISION),
        ('//file2.cc', 'any', None),
        ('//file2.cc', 'fuzzer2', REPORT_REVISION),
      ],
    )

  def test_get_affected_fuzzers_with_exception(self):
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'}
    )
    original_side_effect = self.mock_subprocess_run.side_effect

    def fake_subprocess_run(command, **kwargs):
      if command[:2] == ['gn', 'refs'] and '//file2.cc' in command:
        raise subprocess.CalledProcessError(
          returncode=1, cmd=command, output='out', stderr='err'
        )
      return original_side_effect(command, **kwargs)

    self.mock_subprocess_run.side_effect = fake_subprocess_run
    self._mock_findit(
      {'any': make_findit_response(), 'fuzzer1': make_findit_response()}
    )

    modified_files = ['file1.cc', 'file2.cc']
    all_fuzzers = ['//fuzzer1:fuzzer1']

    affected = find_affected_coverage_guided_fuzzers.get_affected_fuzzers(
      modified_files, 'out/test', all_fuzzers, 'abc'
    )

    # The reverse deps of '//file2.cc' can't be computed, so no fuzzer is
    # reported for it.
    self.assertEqual(
      affected, {'//file1.cc': {'//fuzzer1:fuzzer1'}, '//file2.cc': set()}
    )

  def test_find_affected_fuzzers_for_file_unknown_to_findit(self):
    # No Findit data (e.g., a new file) drops all candidate fuzzers.
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'}
    )
    self._mock_findit({})
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.find_affected_fuzzers_for_file(
        'file1.cc', 'out/test', {'//fuzzer1:fuzzer1'}, 'abc'
      ),
      ('//file1.cc', set()),
    )

  def test_find_affected_fuzzers_for_file_no_fuzzer_covers_changes(self):
    # The CL only changes line 4, which the fuzzers never executed, although
    # they executed lines 3 and 5 around it.
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'},
      diff_tree_stdout=dict(DIFF_TREE_STDOUT, abc=b'@@ -4 +4 @@\n'),
    )
    findit_queries = self._mock_findit({'any': make_findit_response()})
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.find_affected_fuzzers_for_file(
        'file1.cc', 'out/test', {'//fuzzer1:fuzzer1'}, 'abc'
      ),
      ('//file1.cc', set()),
    )
    # Only the aggregated coverage is fetched, not per-fuzzer coverage.
    self.assertEqual(findit_queries, [('//file1.cc', 'any', None)])

  def test_find_affected_fuzzers_for_file_keeps_only_covering_fuzzers(self):
    # Findit looks up FUZZ_TEST coverage by wrapper executable name (without
    # '__generate').
    fuzz_test = '//base:base_unittests_Suite_Test_fuzzer__generate'
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={
        '//file1.cc': f'{fuzz_test}\n//fuzzer1:fuzzer1\n//fuzzer2:fuzzer2\n'
      }
    )
    self._mock_findit(
      {
        'any': make_findit_response(),
        'base_unittests_Suite_Test_fuzzer': make_findit_response(),
        'fuzzer1': make_findit_response(),
        # fuzzer2 only executes line 5, which the CL doesn't change.
        'fuzzer2': dict(
          make_findit_response(),
          data={'line_to_data': [[5, {'line': '}', 'count': 7}]]},
        ),
      }
    )
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.find_affected_fuzzers_for_file(
        'file1.cc',
        'out/test',
        {fuzz_test, '//fuzzer1:fuzzer1', '//fuzzer2:fuzzer2'},
        'abc',
      ),
      ('//file1.cc', {fuzz_test, '//fuzzer1:fuzzer1'}),
    )

  def test_find_affected_fuzzers_for_file_ignores_stale_fuzzer_report(self):
    # Ignore fuzzer1 if it only has a report at an older revision.
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'}
    )
    self._mock_findit(
      {
        'any': make_findit_response(),
        'fuzzer1': make_findit_response(revision=OLD_REPORT_REVISION),
      }
    )
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.find_affected_fuzzers_for_file(
        'file1.cc', 'out/test', {'//fuzzer1:fuzzer1'}, 'abc'
      ),
      ('//file1.cc', set()),
    )

  def test_find_affected_fuzzers_for_file_changes_not_in_report(self):
    # The CL changes lines 1-3. Another CL changed the same lines between the
    # report's commit and the CL's parent, so the report's lines 1-3 hold
    # different code and none of the CL's lines can be looked up. The fuzzers
    # are dropped even though the report shows lines 2 and 3 as covered.
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'},
      diff_tree_stdout={
        **DIFF_TREE_STDOUT,
        REPORT_REVISION: b'@@ -1,3 +1,3 @@\n',
      },
    )
    self._mock_findit(
      {'any': make_findit_response(), 'fuzzer1': make_findit_response()}
    )
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.find_affected_fuzzers_for_file(
        'file1.cc', 'out/test', {'//fuzzer1:fuzzer1'}, 'abc'
      ),
      ('//file1.cc', set()),
    )

  def test_find_affected_fuzzers_for_file_report_is_older_than_parent(self):
    # Prepending a line in the report shifts parent lines 1-3 to 2-4, so hitting
    # only line 4 (parent line 3) still matches.
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'},
      diff_tree_stdout={
        **DIFF_TREE_STDOUT,
        REPORT_REVISION: b'@@ -0,0 +1 @@\n',
      },
    )
    response = dict(
      make_findit_response(),
      data={'line_to_data': [[4, {'line': '  if (G())', 'count': 7}]]},
    )
    self._mock_findit({'any': response, 'fuzzer1': response})
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.find_affected_fuzzers_for_file(
        'file1.cc', 'out/test', {'//fuzzer1:fuzzer1'}, 'abc'
      ),
      ('//file1.cc', {'//fuzzer1:fuzzer1'}),
    )

  def test_get_affected_fuzzers_report_commit_not_fetched(self):
    # Fail instead of guessing if the report's commit is missing locally.
    diff_tree_stdout = dict(DIFF_TREE_STDOUT)
    del diff_tree_stdout[REPORT_REVISION]
    self._set_subprocess_run_side_effect(
      gn_refs_mapping={'//file1.cc': '//fuzzer1:fuzzer1\n'},
      diff_tree_stdout=diff_tree_stdout,
    )
    self._mock_findit({'any': make_findit_response()})
    with self.assertRaises(subprocess.CalledProcessError):
      find_affected_coverage_guided_fuzzers.get_affected_fuzzers(
        ['file1.cc'], 'out/test', ['//fuzzer1:fuzzer1'], 'abc'
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
    self.assertEqual(result, {'//target1:target1'})
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
    self._mock_findit(
      {
        'any': make_findit_response(),
        'fuzzer': make_findit_response(),
      }
    )

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
        'git diff-tree -U0 --text abc~ abc -- file1.cc',
        f'git diff-tree -U0 --text abc~ {REPORT_REVISION} -- file1.cc',
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
    self._mock_findit(
      {
        'any': make_findit_response(),
        'fuzzer1': make_findit_response(),
        'fuzzer2': make_findit_response(),
      }
    )

    with mock.patch('sys.argv', main_argv(out_dir, max_fuzzers=1)):
      self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 0)
    # Both fuzzers cover the changed lines meaning --max-fuzzers drops one.
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
      'base/test.cc', 'base_json_reader_fuzzer', REPORT_REVISION
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
        'revision': [REPORT_REVISION],
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

  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_limits_concurrent_requests(self, mock_urlopen):
    module = find_affected_coverage_guided_fuzzers
    body = json.dumps(make_findit_response()).encode('utf-8')
    lock = threading.Lock()
    active_requests = 0
    max_concurrent_requests = 0
    # Require 2 concurrent requests to prove parallelism without exceeding 2.
    barrier = threading.Barrier(2, timeout=10.0)

    def fake_urlopen(req, timeout):
      nonlocal active_requests, max_concurrent_requests
      with lock:
        active_requests += 1
        max_concurrent_requests = max(max_concurrent_requests, active_requests)
      try:
        barrier.wait()
        # Without the cap, the other requests start in the meantime.
        time.sleep(0.05)
      finally:
        with lock:
          active_requests -= 1
      response = mock.MagicMock()
      response.__enter__.return_value.read.return_value = body
      return response

    mock_urlopen.side_effect = fake_urlopen
    with mock.patch.object(
      module, '_findit_request_semaphore', threading.BoundedSemaphore(2)
    ):
      with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        results = list(
          executor.map(module.get_findit_coverage, ['//base/test.cc'] * 4)
        )

    self.assertTrue(all(results))
    self.assertEqual(max_concurrent_requests, 2)


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
    self.commit('base/f.cc', 'A\nB\nx\ny\nC\nD\nE\nG\nH\n')
    # Only the CL's parent is diffed against the report.
    cl = self.commit('base/g.cc', 'cl\n')
    hunks = find_affected_coverage_guided_fuzzers.get_diff_hunks(
      cl, report, 'base/f.cc'
    )
    self.assertEqual(
      [
        find_affected_coverage_guided_fuzzers.map_old_line_to_new(hunks, line)
        for line in range(1, 10)
      ],
      [1, 2, None, None, 3, 4, 5, 7, 8],
    )

  def test_map_changed_lines_to_report_lines(self):
    # After the report's commit, another CL removed line 'x' from the top and
    # changed 'old' to '2'. So the parent's lines 1 and 3-6 are lines 2 and 4-7
    # in the report, and the parent's line 2 isn't in the report.
    report = self.commit('base/f.cc', 'x\n1\nold\n3\n4\n5\n6\n')
    self.commit('base/f.cc', '1\n2\n3\n4\n5\n6\n')
    cl = self.commit(
      'base/f.cc',
      'top\n'  # Inserted at the top.
      '1\n'
      'new2\n'  # Line 2 modified.
      '3\n'
      'after3\n'  # Inserted after line 3.
      '4\n'
      'new5\n'  # Line 5 modified.
      '6\n',
    )
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.map_changed_lines_to_report_lines(
        find_affected_coverage_guided_fuzzers.ChangedLines(
          modified={2, 5}, insertions={0, 3}
        ),
        cl,
        report,
        'base/f.cc',
      ),
      # Line 5 is line 6 in the report, and line 2 changed since, so it's
      # skipped. The insertion at the top is looked up by line 1 (line 2 in
      # the report) and the one after line 3 by lines 3 and 4 (lines 4 and 5).
      {2, 4, 5, 6},
    )

  def test_get_diff_hunks_binary_looking_file(self):
    # Without --text, git prints no hunks for a file with a NUL byte, so the
    # file looks unchanged.
    self.commit('base/f.cc', 'a\0\nb\n')
    cl = self.commit('base/f.cc', 'a\0\nB\n')
    self.assertEqual(
      find_affected_coverage_guided_fuzzers.get_diff_hunks(cl, cl, 'base/f.cc'),
      [
        find_affected_coverage_guided_fuzzers.DiffHunk(
          old_start=2, old_count=1, new_count=1
        )
      ],
    )

  def test_get_diff_hunks_missing_revision(self):
    self.commit('base/f.cc', 'a\n')  # So that cl has a parent.
    cl = self.commit('base/f.cc', 'b\n')
    # E.g. a coverage report's commit that the checkout hasn't fetched.
    with self.assertRaises(subprocess.CalledProcessError):
      find_affected_coverage_guided_fuzzers.get_diff_hunks(
        cl, '1234567890abcdef1234567890abcdef12345678', 'base/f.cc'
      )


if __name__ == '__main__':
  unittest.main()
