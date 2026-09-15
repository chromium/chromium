#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import sys
import unittest
from unittest import mock
import urllib.error
from urllib.parse import parse_qs
from urllib.parse import urlparse

# Add the directory containing the script to the path so we can import it
sys.path.append(os.path.dirname(os.path.realpath(__file__)))
import find_affected_coverage_guided_fuzzers


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

  def test_generate_gn_build_dir_default(self):
    find_affected_coverage_guided_fuzzers.generate_gn_build_dir('out/test')

    expected_args_str = (
      find_affected_coverage_guided_fuzzers.gn_args_libfuzzer.replace('\n', ' ')
    )
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      [f'gn gen out/test --args={expected_args_str}'],
    )

  def test_get_modified_files(self):
    self.mock_subprocess_run.return_value.stdout = (
      'file1.cc\nfile2.h\nfile3.txt\n'
    )
    files = find_affected_coverage_guided_fuzzers.get_modified_files()
    self.assertEqual(files, ['file1.cc', 'file2.h'])

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

  def test_main_no_modified_files(self):
    self.mock_subprocess_run.return_value.stdout = ''
    with mock.patch('sys.argv', ['script.py', '--out-dir', 'out/test']):
      self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 0)
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      ['git diff --name-only --diff-filter=d HEAD~'],
    )

  def test_main_no_fuzzers(self):
    self._set_subprocess_run_side_effect(git_diff_stdout='file1.cc\n')

    with mock.patch('sys.argv', ['script.py', '--out-dir', 'out/test']):
      with self.assertLogs(level='WARNING') as cm:
        self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 1)
    self.assertIn('No fuzzer targets found', cm.output[0])

    expected_args_str = (
      find_affected_coverage_guided_fuzzers.gn_args_libfuzzer.replace('\n', ' ')
    )
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      [
        'git diff --name-only --diff-filter=d HEAD~',
        f'gn gen out/test --args={expected_args_str}',
        'gn refs out/test //testing/libfuzzer:fuzzing_engine '
        '--all --type=executable --as=label -q',
      ],
    )

  def test_main_success(self):
    self._set_subprocess_run_side_effect(
      git_diff_stdout='file1.cc\n',
      gn_refs_mapping={
        '//testing/libfuzzer:fuzzing_engine': '//fuzzer:fuzzer\n',
        '//file1.cc': '//fuzzer:fuzzer\n',
      },
    )

    with mock.patch('sys.argv', ['script.py', '--out-dir', 'out/test']):
      # Redirect stdout to avoid printing during tests
      with mock.patch('sys.stdout', new_callable=mock.MagicMock) as mock_stdout:
        self.assertEqual(find_affected_coverage_guided_fuzzers.main(), 0)

        # Verify the printed JSON output is correct
        output_str = ''.join(
          call.args[0] for call in mock_stdout.write.call_args_list
        )
        output_data = json.loads(output_str)
        self.assertEqual(
          output_data, {'affected_fuzzers': {'//file1.cc': ['//fuzzer:fuzzer']}}
        )

    expected_args_str = (
      find_affected_coverage_guided_fuzzers.gn_args_libfuzzer.replace('\n', ' ')
    )
    self.assertEqual(
      self.get_calls_args(self.mock_subprocess_run),
      [
        'git diff --name-only --diff-filter=d HEAD~',
        f'gn gen out/test --args={expected_args_str}',
        'gn refs out/test //testing/libfuzzer:fuzzing_engine '
        '--all --type=executable --as=label -q',
        'gn refs out/test --all --as=label -q //file1.cc',
      ],
    )

  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_success(self, mock_urlopen):
    lines = [
      {'first': 1, 'last': 10, 'count': 5},
      {'first': 12, 'last': 12, 'count': 0},
    ]
    fake_response = mock.MagicMock()
    fake_response.read.return_value = json.dumps(
      {'data': {'metadata': {'lines': lines}}}
    ).encode('utf-8')
    mock_urlopen.return_value.__enter__.return_value = fake_response

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      'base/json/json_reader.cc'
    )
    self.assertEqual(result, lines)

    req = mock_urlopen.call_args[0][0]
    query_params = parse_qs(urlparse(req.full_url).query)
    self.assertEqual(query_params.get('path'), ['//base/json/json_reader.cc'])
    self.assertEqual(query_params.get('platform'), ['fuzz'])
    self.assertEqual(query_params.get('test_suite_type'), ['any'])
    self.assertEqual(query_params.get('raw'), ['true'])

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
    lines = [{'first': 1, 'last': 2, 'count': 3}]
    fake_response = mock.MagicMock()
    fake_response.read.return_value = json.dumps(
      {'data': {'metadata': {'lines': lines}}}
    ).encode('utf-8')
    mock_urlopen.side_effect = [
      urllib.error.HTTPError(
        'http://example.com', 500, 'Server Error', {}, None
      ),
      mock.MagicMock(__enter__=mock.MagicMock(return_value=fake_response)),
    ]

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/test.cc'
    )

    self.assertEqual(result, lines)
    self.assertEqual(mock_urlopen.call_count, 2)
    self.assertEqual(mock_sleep.call_count, 1)

  @mock.patch('find_affected_coverage_guided_fuzzers.time.sleep')
  @mock.patch('find_affected_coverage_guided_fuzzers.urllib.request.urlopen')
  def test_get_findit_coverage_gives_up_after_max_retries(
    self, mock_urlopen, mock_sleep
  ):
    mock_urlopen.side_effect = urllib.error.HTTPError(
      'http://example.com', 500, 'Server Error', {}, None
    )

    result = find_affected_coverage_guided_fuzzers.get_findit_coverage(
      '//base/test.cc'
    )

    max_retries = find_affected_coverage_guided_fuzzers.MAX_FINDIT_API_RETRIES
    self.assertIsNone(result)
    self.assertEqual(mock_urlopen.call_count, max_retries)
    self.assertEqual(mock_sleep.call_count, max_retries - 1)

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


if __name__ == '__main__':
  unittest.main()
