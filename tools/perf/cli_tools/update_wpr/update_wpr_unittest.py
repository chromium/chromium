# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import argparse
import os
import re
import unittest
from unittest import mock

import six

from cli_tools.update_wpr import update_wpr
from core import cli_helpers
from core.services import request


WPR_UPDATER = 'cli_tools.update_wpr.update_wpr.'

BUILTIN_MODULE = '__builtin__' if six.PY2 else 'builtins'

# This is \\<story\\> in Python 2 and <story> in Python 3.
ESCAPED_STORY = re.escape('<story>')

def mock_exists(path):
  return '<archive>' in path

class UpdateWprTest(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None

    self._check_log = mock.patch('core.cli_helpers.CheckLog').start()
    self._run = mock.patch('core.cli_helpers.Run').start()
    self._check_output = mock.patch('subprocess.check_output').start()
    self._check_call = mock.patch('subprocess.check_call').start()
    self._info = mock.patch('core.cli_helpers.Info').start()
    self._comment = mock.patch('core.cli_helpers.Comment').start()
    self._open = mock.patch(BUILTIN_MODULE + '.open').start()
    datetime = mock.patch('datetime.datetime').start()
    datetime.now.return_value.strftime.return_value = '<tstamp>'

    mock.patch('tempfile.mkdtemp', return_value='/tmp/dir').start()
    mock.patch('random.randint', return_value=1234).start()
    mock.patch('core.cli_helpers.Fatal').start()
    mock.patch('core.cli_helpers.Error').start()
    mock.patch('core.cli_helpers.Step').start()
    mock.patch('os.environ').start().copy.return_value = {}
    mock.patch(WPR_UPDATER + 'SRC_ROOT', '...').start()
    mock.patch(WPR_UPDATER + 'RESULTS2JSON', '.../results2json').start()
    mock.patch(WPR_UPDATER + 'HISTOGRAM2CSV', '.../histograms2csv').start()
    mock.patch(WPR_UPDATER + 'RUN_BENCHMARK', '.../run_benchmark').start()
    mock.patch(WPR_UPDATER + 'DATA_DIR', '.../data/dir').start()
    mock.patch(WPR_UPDATER + 'RECORD_WPR', '.../record_wpr').start()
    mock.patch(WPR_UPDATER + 'WprUpdater._LoadArchiveInfo').start()
    mock.patch(WPR_UPDATER + 'CrossbenchWprUpdater._find_browser').start()
    mock.patch('os.path.join', lambda *parts: '/'.join(parts)).start()
    mock.patch('os.path.exists', return_value=True).start()
    mock.patch('time.sleep').start()

    self.wpr_updater = update_wpr.WprUpdater(argparse.Namespace(
      story='<story>', device_id=None, repeat=1, binary=None, bug_id=None,
      reviewers=['someone@chromium.org'],
      bss='desktop_system_health_story_set'))

  def tearDown(self):
    mock.patch.stopall()

  @mock.patch(WPR_UPDATER + 'WprUpdater')
  def testMain(self, wpr_updater_cls):
    update_wpr.Main([
      '-s', 'foo:bar:story:2019',
      '-d', 'H2345234FC33',
      '-bss', 'mobile_system_health_story_set',
      '--binary', '<binary>',
      '-b', '1234',
      '-r', 'test_user1@chromium.org',
      '-r', 'test_user2@chromium.org',
      'live',
    ])
    self.assertListEqual(wpr_updater_cls.mock_calls, [
        mock.call(
            argparse.Namespace(binary='<binary>',
                               command='live',
                               device_id='H2345234FC33',
                               repeat=1,
                               story='foo:bar:story:2019',
                               bug_id='1234',
                               cb_wprgo_file=None,
                               bss='mobile_system_health_story_set',
                               is_cb=False,
                               output_dir=None,
                               reviewers=[
                                   'test_user1@chromium.org',
                                   'test_user2@chromium.org'
                               ])),
        mock.call().LiveRun(),
        mock.call().Cleanup(),
    ])

  @mock.patch('shutil.rmtree')
  @mock.patch('core.cli_helpers.Ask', return_value=False)
  def testCleanupManual(self, ask, rmtree):
    del ask  # Unused.
    self.wpr_updater.Cleanup()
    self._comment.assert_called_once_with(
        'No problem. All logs will remain in /tmp/dir - feel free to remove '
        'that directory when done.')
    rmtree.assert_not_called()

  @mock.patch('shutil.rmtree')
  @mock.patch('core.cli_helpers.Ask', return_value=True)
  def testCleanupAutomatic(self, ask, rmtree):
    del ask  # Unused.
    self.wpr_updater.created_branch = 'foo'
    self.wpr_updater.Cleanup()
    rmtree.assert_called_once_with('/tmp/dir', ignore_errors=True)

  def testGetBranchName(self):
    self._check_output.return_value = 'master\n'
    self.assertEqual(update_wpr._GetBranchName(), 'master')
    self._check_output.assert_called_once_with(
        ['git', 'rev-parse', '--abbrev-ref', 'HEAD'])

  def testCreateBranch(self):
    self.wpr_updater._CreateBranch()
    self._run.assert_called_once_with(
        ['git', 'new-branch', 'update-wpr--story--1234'])

  def testSendCLForReview(self):
    update_wpr._SendCLForReview('comment')
    self._check_call.assert_called_once_with(
        ['git', 'cl', 'comments', '--publish', '--add-comment', 'comment'])

  @mock.patch('os.dup')
  @mock.patch('os.close')
  @mock.patch('os.dup2')
  @mock.patch('webbrowser.open')
  def testOpenBrowser(self, webbrowser_open, os_dup2, os_close, os_dup):
    del os_dup2, os_close, os_dup  # Unused.
    update_wpr._OpenBrowser('<url>')
    webbrowser_open.assert_called_once_with('<url>')

  # Mock low-level methods tested above.
  @mock.patch(WPR_UPDATER + '_GetBranchName', return_value='HEAD')
  @mock.patch(WPR_UPDATER + 'WprUpdater._GetBranchIssueUrl',
              return_value='<issue-url>')
  @mock.patch(WPR_UPDATER + 'WprUpdater._CreateBranch')
  @mock.patch(WPR_UPDATER + '_SendCLForReview')
  @mock.patch(WPR_UPDATER + '_OpenBrowser')
  # Mock high-level methods tested below.
  @mock.patch(WPR_UPDATER + 'WprUpdater.LiveRun')
  @mock.patch(WPR_UPDATER + 'WprUpdater.RecordWpr')
  @mock.patch(WPR_UPDATER + 'WprUpdater.ReplayWpr')
  @mock.patch(WPR_UPDATER + 'WprUpdater.UploadWpr', return_value=True)
  @mock.patch(WPR_UPDATER + 'WprUpdater.UploadCL', return_value=0)
  @mock.patch(WPR_UPDATER + 'WprUpdater.StartPinpointJobs',
              return_value=(['<url1>', '<url2>', '<url3>'], []))
  # Mock user interaction.
  @mock.patch('core.cli_helpers.Ask', side_effect=[
      True,         # Should script create a new branch automatically?
      'continue',   # Should I continue with recording, ...?
      'continue'])  # Should I record and replay again, ...?
  def testAutoRun(
      self, ask, start_pinpoint_jobs, upload_cl, upload_wpr, replay_wpr,
      record_wpr, live_run, open_browser, send_cl_for_review, create_branch,
      get_branch_issue_url, get_branch_name):
    del ask, create_branch, get_branch_issue_url, get_branch_name  # Unused.
    self.wpr_updater.AutoRun()

    # Run once to make sure story works.
    live_run.assert_called_once_with()
    # Run again to create a recording.
    record_wpr.assert_called_once_with()
    # Replay to verify the recording.
    replay_wpr.assert_called_once_with()
    # Upload the recording.
    upload_wpr.assert_called_once_with()
    # Upload the CL.
    upload_cl.assert_called_once_with(short_description=False)
    # Start pinpoint jobs to verify recording works on the bots.
    start_pinpoint_jobs.assert_called_once_with(None)
    # Send CL for review with a comment listing triggered Pinpoint jobs.
    send_cl_for_review.assert_called_once_with(
        'Started the following Pinpoint jobs:\n'
        '  - <url1>\n'
        '  - <url2>\n'
        '  - <url3>')
    # Open the CL in browser,
    open_browser.assert_called_once_with('<issue-url>')

  @mock.patch(WPR_UPDATER + 'WprUpdater._RunBenchmark',
              return_value='<out-file>')
  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  def testLiveRun(self, print_run_info, run_benchmark):
    self.wpr_updater.LiveRun()
    run_benchmark.assert_called_once_with(log_name='live', live=True)
    print_run_info.assert_called_once_with('<out-file>', chrome_log_file=True)

  def checkRunBenchmark(self, benchmark):
    self._check_log.assert_called_once_with(
        [
            '.../run_benchmark', 'run', '--browser=system',
            benchmark, '--output-format=html',
            '--show-stdout', '--reset-results',
            '--story-filter=^%s$' % ESCAPED_STORY,
            '--browser-logging-verbosity=verbose', '--pageset-repeat=1',
            '--output-dir', '/tmp/dir', '--also-run-disabled-tests',
            '--legacy-json-trace-format', '--use-live-sites'
        ],
        env={'LC_ALL': 'en_US.UTF-8'},
        log_path='/tmp/dir/<log_name>_<tstamp>')


  @mock.patch('os.rename')
  def testRunBenchmarkMemoryDesktop(self, rename):
    self._check_output.return_value = '  <chrome-log>'

    self.wpr_updater._RunBenchmark('<log_name>', True)

    self.checkRunBenchmark('system_health.memory_desktop')

    # Check logs are correctly extracted.
    self.assertListEqual(rename.mock_calls, [
      mock.call(
        '/tmp/dir/results.html', '/tmp/dir/<log_name>_<tstamp>.results.html'),
      mock.call('<chrome-log>', '/tmp/dir/<log_name>_<tstamp>.chrome.log'),
    ])

  @mock.patch('os.rename')
  def testRunBenchmarkMemoryMobile(self, rename):
    del rename
    self.wpr_updater = update_wpr.WprUpdater(argparse.Namespace(
      story='<story>', device_id=None, repeat=1, binary=None, bug_id=None,
      reviewers=['someone@chromium.org'],
      bss='mobile_system_health_story_set'))
    self.wpr_updater._RunBenchmark('<log_name>', True)

    self.checkRunBenchmark('system_health.memory_mobile')

  @mock.patch('os.rename')
  def testRunBenchmarkOther(self, rename):
    del rename
    self.wpr_updater = update_wpr.WprUpdater(argparse.Namespace(
      story='<story>', device_id=None, repeat=1, binary=None, bug_id=None,
      reviewers=['someone@chromium.org'],
      bss='other'))
    self.wpr_updater._RunBenchmark('<log_name>', True)

    self.checkRunBenchmark('other')

  def testPrintResultsHTMLInfo(self):
    self._open.return_value.__enter__.return_value.readlines.return_value = [
        'console:error:network,foo,bar',
        'console:error:js,foo,bar',
        'console:error:security,foo,bar',
    ]
    update_wpr._PrintResultsHTMLInfo('<outfile>')
    self.assertListEqual(self._run.mock_calls, [
      mock.call(
        ['.../results2json', '<outfile>.results.html', '<outfile>.hist.json'],
        env={'LC_ALL': 'en_US.UTF-8'}),
      mock.call(
        ['.../histograms2csv', '<outfile>.hist.json', '<outfile>.hist.csv'],
        env={'LC_ALL': 'en_US.UTF-8'}),
    ])
    self._open.assert_called_once_with('<outfile>.hist.csv')
    self.assertListEqual(self._info.mock_calls, [
      mock.call(
        'Metrics results: file://{path}', path='<outfile>.results.html'),
      mock.call('    [console:error:network]:  bar'),
      mock.call('    [console:error:js]:       bar'),
      mock.call('    [console:error:security]: bar')
    ])

  def testCountLogLines(self):
    self._open.return_value.__enter__.return_value = [
        'foo yy', 'xx foobar', 'baz']
    self.assertEqual(update_wpr._CountLogLines('<out-file>', 'foo.*'), 2)
    self._open.assert_called_once_with('<out-file>')

  def testExtractMissingURLsFromLog(self):
    self._open.return_value.__enter__.return_value = [
      'some-timestamp [network]: Failed to load resource: the server responded '
      'with a status of 404 () http://www.google.com/1\n',
      '[network]: Failed to load resource: the server responded with a status '
      'of 404 () https://www.google.com/2 foobar\n',
      'foobar',
    ]
    self.assertListEqual(
        update_wpr._ExtractMissingURLsFromLog('<log-file>'),
        ['http://www.google.com/1', 'https://www.google.com/2'])
    self._open.assert_called_once_with('<log-file>')

  @mock.patch(WPR_UPDATER + '_PrintResultsHTMLInfo', side_effect=[Exception()])
  @mock.patch(WPR_UPDATER + '_CountLogLines', return_value=0)
  def testPrintRunInfo(self, count_log_lines, print_results):
    del count_log_lines  # Unused.
    self._check_output.return_value = '0\n'
    update_wpr._PrintRunInfo(
        '<outfile>', chrome_log_file=True, results_details=True)
    print_results.assert_called_once_with('<outfile>')
    self.assertListEqual(self._info.mock_calls, [
      mock.call('Stdout/Stderr Log: <outfile>'),
      mock.call('Chrome Log: <outfile>.chrome.log'),
      mock.call('    Total output:   0'),
      mock.call('    Total Console:  0'),
      mock.call('    [security]:     0'),
      mock.call('    [network]:      0'),
    ])


  @mock.patch('json.load', return_value={'issue_url': '<url>'})
  def testGetBranchIssueUrl(self, json_load):
    del json_load  # Unused.
    self.assertEqual(self.wpr_updater._GetBranchIssueUrl(), '<url>')
    self._check_output.assert_called_once_with([
      'git', 'cl', 'issue', '--json', '/tmp/dir/git_cl_issue.json'])

  @mock.patch('core.cli_helpers.Ask', side_effect=[
      'yes'])
  @mock.patch('os.remove')
  def testDeleteExistingWprYes(self, os_remove, ask):
    del ask
    wpr_archive_info = mock.Mock()
    wpr_archive_info.data = {
      'archives': {
        '<story>': {
          'DEFAULT': '<archive>',
          'mac': 'other_archive'
          },
      }
    }
    self.wpr_updater.wpr_archive_info = wpr_archive_info

    self.wpr_updater._DeleteExistingWpr()
    self.assertListEqual(os_remove.mock_calls, [
      mock.call('.../data/dir/<archive>'),
      mock.call('.../data/dir/<archive>.sha1'),
      mock.call('.../data/dir/other_archive'),
      mock.call('.../data/dir/other_archive.sha1')
    ])
    self.wpr_updater.wpr_archive_info.RemoveStory.assert_called()

  @mock.patch('core.cli_helpers.Ask', side_effect=[
      'no'])
  @mock.patch('os.remove')
  def testDeleteExistingWprNo(self, os_remove, ask):
    del ask
    wpr_archive_info = mock.Mock()
    wpr_archive_info.data = {
      'archives': {
        '<story>': {
          'DEFAULT': '<archive>',
          'mac': 'other_archive'
          },
      }
    }
    self.wpr_updater.wpr_archive_info = wpr_archive_info

    self.wpr_updater._DeleteExistingWpr()
    self.assertListEqual(os_remove.mock_calls, [])
    self.wpr_updater.wpr_archive_info.RemoveStory.assert_not_called()

  @mock.patch('core.cli_helpers.Ask', side_effect=[
      'yes'])
  @mock.patch('os.remove')
  def testDoesNotDeleteReusedWpr(self, os_remove, ask):
    del ask
    wpr_archive_info = mock.Mock()
    wpr_archive_info.data = {
      'archives': {
        '<story>': {'DEFAULT': '<archive>'},
        '<other>': {'DEFAULT': 'foo', 'linux': '<archive>'}
      }
    }
    self.wpr_updater.wpr_archive_info = wpr_archive_info

    self.wpr_updater._DeleteExistingWpr()
    os_remove.assert_not_called()

  @mock.patch(WPR_UPDATER + '_ExtractMissingURLsFromLog',
              return_value=['foo', 'bar'])
  @mock.patch(WPR_UPDATER + 'WprUpdater._GetWprArchivePathsAndUsageForStory',
              return_value=[('<archive>', False)])
  @mock.patch('py_utils.binary_manager.BinaryManager', autospec=True)
  def testAddMissingURLsToArchive(self, bmanager, existing_wpr, extract_mock):
    del existing_wpr   # Unused.

    wpr_archive_info = mock.Mock()
    wpr_archive_info.data = {
      'archives': {
        '<story>': {'DEFAULT': '<archive>'},
      }
    }
    self.wpr_updater.wpr_archive_info = wpr_archive_info

    bmanager.return_value.FetchPath.return_value = '<wpr-go-bin>'
    self.wpr_updater._AddMissingURLsToArchive('<replay-log>')
    extract_mock.assert_called_once_with('<replay-log>')
    self._check_call.assert_called_once_with(
        ['<wpr-go-bin>', 'add', '<archive>', 'foo', 'bar'])

  @mock.patch('core.cli_helpers.Ask', side_effect=[
      'other_archive'])
  @mock.patch(WPR_UPDATER + '_ExtractMissingURLsFromLog',
              return_value=['foo', 'bar'])
  @mock.patch(WPR_UPDATER + 'WprUpdater._GetWprArchivePathsAndUsageForStory',
              return_value=[('other_archive', False)])
  @mock.patch('py_utils.binary_manager.BinaryManager', autospec=True)
  def testAddMissingURLsToArchiveMulti(self, bmanager,
                                       existing_wpr, extract_mock, ask):
    del existing_wpr, ask   # Unused.

    wpr_archive_info = mock.Mock()
    wpr_archive_info.data = {
      'archives': {
        '<story>': {
          'DEFAULT': '<archive>',
          'mac': 'other_archive'
        }
      }
    }
    self.wpr_updater.wpr_archive_info = wpr_archive_info

    bmanager.return_value.FetchPath.return_value = '<wpr-go-bin>'
    self.wpr_updater._AddMissingURLsToArchive('<replay-log>')
    extract_mock.assert_called_once_with('<replay-log>')
    self._check_call.assert_called_once_with(
        ['<wpr-go-bin>', 'add', 'other_archive', 'foo', 'bar'])

  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  @mock.patch(WPR_UPDATER + 'WprUpdater._DeleteExistingWpr')
  def testRecordWprDesktop(self, delete_existing_wpr, print_run_info):
    del delete_existing_wpr  # Unused.
    self.wpr_updater.RecordWpr()
    self._check_log.assert_called_once_with([
        '.../record_wpr',
        'desktop_system_health_story_set',
        '--story-filter=^%s$' % ESCAPED_STORY, '--browser=system'
    ],
                                            env={'LC_ALL': 'en_US.UTF-8'},
                                            log_path='/tmp/dir/record_<tstamp>')
    print_run_info.assert_called_once_with(
        '/tmp/dir/record_<tstamp>', chrome_log_file=True, results_details=False)

  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  @mock.patch(WPR_UPDATER + 'WprUpdater._DeleteExistingWpr')
  def testRecordWprMobile(self, delete_existing_wpr, print_run_info):
    del delete_existing_wpr  # Unused.
    self.wpr_updater = update_wpr.WprUpdater(argparse.Namespace(
      story='<story>', device_id=None, repeat=1, binary=None, bug_id=None,
      reviewers=['someone@chromium.org'], bss='mobile_system_health_story_set'))
    self.wpr_updater.device_id = '<serial>'
    self.wpr_updater.RecordWpr()
    self._check_log.assert_called_once_with([
        '.../record_wpr',
        'mobile_system_health_story_set',
        '--story-filter=^%s$' % ESCAPED_STORY,
        '--browser=android-system-chrome', '--device=<serial>'
    ],
                                            env={'LC_ALL': 'en_US.UTF-8'},
                                            log_path='/tmp/dir/record_<tstamp>')
    print_run_info.assert_called_once_with(
        '/tmp/dir/record_<tstamp>', chrome_log_file=False,
        results_details=False)

  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  @mock.patch(WPR_UPDATER + 'WprUpdater._RunBenchmark',
              return_value='<out-file>')
  def testReplayWpr(self, run_benchmark, print_run_info):
    self.wpr_updater.ReplayWpr()
    run_benchmark.assert_called_once_with(log_name='replay', live=False)
    print_run_info.assert_called_once_with('<out-file>', chrome_log_file=True)

  @mock.patch(WPR_UPDATER + 'WprUpdater._GetWprArchivePathsAndUsageForStory',
              return_value=[('<archive>', False), ('other', False)])
  @mock.patch('os.path.exists', side_effect=mock_exists)
  def testUploadWPR(self, existing_wpr, exists):
    del existing_wpr, exists  # Unused.

    self.wpr_updater.UploadWpr()
    self.assertListEqual(self._run.mock_calls, [
      mock.call(['upload_to_google_storage.py',
                 '--bucket=chrome-partner-telemetry', '<archive>']),
      mock.call(['git', 'add', '<archive>.sha1'])
    ])

  @mock.patch('subprocess.call', return_value=1)
  def testUploadCL(self, subprocess_call):
    del subprocess_call  # Unused.
    self._run.return_value = 42
    self.assertEqual(self.wpr_updater.UploadCL(), 42)
    self.assertListEqual(self._run.mock_calls, [
      mock.call([
        'git', 'commit', '-a', '-m', 'Add <story> system health story\n\n'
        'This CL was created automatically with tools/perf/update_wpr script'
      ]),
      mock.call([
        'git', 'cl', 'upload', '--reviewers', 'someone@chromium.org',
        '--force', '--message-file', '/tmp/dir/commit_message.tmp'
      ], ok_fail=True),
    ])

  @mock.patch(WPR_UPDATER + 'WprUpdater._GetBranchIssueUrl',
              return_value='<issue-url>')
  @mock.patch('core.services.pinpoint_service.NewJob',
              return_value={'jobUrl': '<url>'})
  def testStartPinPointJobsDesktop(self, new_job, get_branch_issue_url):
    del get_branch_issue_url  # Unused.
    self.assertEqual(
        self.wpr_updater.StartPinpointJobs(),
        (['<url>', '<url>', '<url>'], []))
    new_job.assert_called_with(
        base_git_hash='HEAD',
        target='performance_test_suite',
        patch='<issue-url>',
        bug_id='',
        story='<story>',
        extra_test_args='--pageset-repeat=1',
        configuration='mac-10_12_laptop_low_end-perf',
        benchmark='system_health.common_desktop')
    self.assertEqual(new_job.call_count, 3)

  @mock.patch(WPR_UPDATER + 'WprUpdater._GetTargetFromConfiguration',
              return_value='performance_test_suite')
  @mock.patch(WPR_UPDATER + 'WprUpdater._GetBranchIssueUrl',
              return_value='<issue-url>')
  @mock.patch('core.services.pinpoint_service.NewJob',
              side_effect=request.ServerError(
                  mock.Mock(), mock.Mock(status=500), ''))
  def testStartPinPointJobsMobileFail(self, new_job, get_branch_issue_url,
                                      get_target):
    del get_branch_issue_url  # Unused.
    self.wpr_updater.device_id = '<serial>'
    self.assertEqual(
        self.wpr_updater.StartPinpointJobs(['<config>']), ([], ['<config>']))
    new_job.assert_called_once_with(
        base_git_hash='HEAD',
        target='performance_test_suite',
        patch='<issue-url>',
        bug_id='',
        story='<story>',
        extra_test_args='--pageset-repeat=1',
        configuration='<config>',
        benchmark='system_health.common_mobile')
    get_target.assert_called_once_with('<config>')

  @mock.patch('core.services.pinpoint_service.NewJob',
              side_effect=request.ServerError(mock.Mock(),
                                              mock.Mock(status=500), ''))
  def testStartPinpointJobsInvalidConfig(self, new_job):
    with self.assertRaises(RuntimeError):
      self.wpr_updater.StartPinpointJobs(['<config>'])
    new_job.assert_not_called()

  def _CreateCrossbenchWprUpdater(self):
    update_wpr.CrossbenchWprUpdater._LoadArchiveInfo = mock.MagicMock()
    with mock.patch('os.path.exists', return_value=False), \
        mock.patch('pathlib.Path'):
      updater = update_wpr.CrossbenchWprUpdater(
          argparse.Namespace(story='test_story',
                             device_id=None,
                             repeat=1,
                             binary=None,
                             bug_id=None,
                             reviewers=None,
                             cb_wprgo_file=None,
                             is_cb=True,
                             output_dir='/tmp/dir',
                             bss='test_benchmark'))
      updater.browser = 'browser_test_type'
      return updater

  def _CrossbenchCommonExpectedCommand(self, cb_path):
    return update_wpr.PY_EXECUTABLE + [
        update_wpr.CrossbenchWprUpdater._CB_TOOL,
        'test_benchmark',
        '--repeat=1',
        '--browser=adb:browser_test_type',
        '--verbose',
        '--debug',
        '--no-symlinks',
        f'--out-dir=/tmp/dir/<tstamp>/{cb_path}',
    ]

  def testCrossbenchRecordWpr(self):
    update_wpr.CrossbenchWprUpdater._LoadArchiveInfo = mock.MagicMock()
    self.wpr_updater = self._CreateCrossbenchWprUpdater()
    self.wpr_updater.RecordWpr()
    self._check_log.assert_called_once_with(
        self._CrossbenchCommonExpectedCommand('cb_record') + [
            '--probe=wpr',
            '--story=test_story',
        ],
        log_path='/tmp/dir/<tstamp>/record.log',
        env={'LC_ALL': 'en_US.UTF-8'},
    )

  @mock.patch('os.path.exists', return_value=True)
  def testCrossbenchReplayWpr(self, _):
    self.wpr_updater = self._CreateCrossbenchWprUpdater()
    self.wpr_updater.ReplayWpr('/foo/bar.wprgo')
    self._check_log.assert_called_once_with(
        self._CrossbenchCommonExpectedCommand('cb_replay') + [
            '--network={type:"wpr", path:"/foo/bar.wprgo"}',
            '--story=test_story',
        ],
        log_path='/tmp/dir/<tstamp>/replay.log',
        env={'LC_ALL': 'en_US.UTF-8'},
    )

  @mock.patch.object(update_wpr.CrossbenchWprUpdater,
                     '_GetDataWprArchivePath',
                     return_value='/foo/bar.wprgo')
  @mock.patch.object(update_wpr.CrossbenchWprUpdater, '_CopyTempWprgoToData')
  @mock.patch.object(os.path, 'exists', return_value=True)
  def testCrossbenchUploadWPR(self, existing_wpr, copy_wpr, exists):
    del existing_wpr, copy_wpr, exists  # Unused.

    self.wpr_updater = self._CreateCrossbenchWprUpdater()
    self.wpr_updater.UploadWpr()
    self.assertListEqual(self._run.mock_calls, [
        mock.call([
            'upload_to_google_storage.py', '--bucket=chrome-partner-telemetry',
            '/foo/bar.wprgo'
        ]),
        mock.call(['git', 'add', '/foo/bar.wprgo.sha1'])
    ])

  def testCrossbenchLiveRun(self):
    self.wpr_updater = self._CreateCrossbenchWprUpdater()
    self.wpr_updater.LiveRun()
    self._check_log.assert_called_once_with(
        self._CrossbenchCommonExpectedCommand('cb_live') + [
            '--story=test_story',
        ],
        log_path='/tmp/dir/<tstamp>/live.log',
        env={'LC_ALL': 'en_US.UTF-8'},
    )

  @mock.patch.object(update_wpr.CrossbenchWprUpdater,
                     'RecordWpr',
                     return_value='/foo/bar.wprgo')
  @mock.patch.object(update_wpr.CrossbenchWprUpdater, 'ReplayWpr')
  @mock.patch.object(update_wpr.CrossbenchWprUpdater,
                     'UploadWpr',
                     return_value=True)
  @mock.patch.object(
      cli_helpers,
      'Ask',
      side_effect=[
          True,  # Do you want to continue?
          'continue'
      ])  # Are you sure to upload the WPR?
  def testCrossbenchAutoRun(self, ask, upload_wpr, replay_wpr, record_wpr):
    del ask  # Unused.
    self.wpr_updater = self._CreateCrossbenchWprUpdater()

    self.wpr_updater.AutoRun()

    record_wpr.assert_called_once_with()
    replay_wpr.assert_called_once_with('/foo/bar.wprgo')
    upload_wpr.assert_called_once_with('/foo/bar.wprgo')

if __name__ == "__main__":
  unittest.main()
