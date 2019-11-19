# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import argparse
import unittest

import mock

from cli_tools.update_wpr import update_wpr
from core.services import request


WPR_UPDATER = 'cli_tools.update_wpr.update_wpr.'


class UpdateWprTest(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None

    self._check_log = mock.patch('core.cli_helpers.CheckLog').start()
    self._run = mock.patch('core.cli_helpers.Run').start()
    self._check_output = mock.patch('subprocess.check_output').start()
    self._check_call = mock.patch('subprocess.check_call').start()
    self._info = mock.patch('core.cli_helpers.Info').start()
    self._comment = mock.patch('core.cli_helpers.Comment').start()
    self._open = mock.patch('__builtin__.open').start()
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
    mock.patch('os.path.join', lambda *parts: '/'.join(parts)).start()
    mock.patch('os.path.exists', return_value=True).start()
    mock.patch('time.sleep').start()

    self.wpr_updater = update_wpr.WprUpdater(argparse.Namespace(
      story='<story>', device_id=None, repeat=1, binary=None, bug_id=None,
      reviewers=['someone@chromium.org']))

  def tearDown(self):
    mock.patch.stopall()

  @mock.patch(WPR_UPDATER + 'WprUpdater')
  def testMain(self, wpr_updater_cls):
    update_wpr.Main([
      '-s', 'foo:bar:story:2019',
      '-d', 'H2345234FC33',
      '--binary', '<binary>',
      '-b', '1234',
      '-r', 'test_user1@chromium.org',
      '-r', 'test_user2@chromium.org',
      'live',
    ])
    self.assertListEqual(wpr_updater_cls.mock_calls, [
      mock.call(argparse.Namespace(
        binary='<binary>', command='live', device_id='H2345234FC33',
        repeat=1, story='foo:bar:story:2019', bug_id='1234',
        reviewers=['test_user1@chromium.org', 'test_user2@chromium.org'])),
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

  @mock.patch(WPR_UPDATER + 'WprUpdater._RunSystemHealthMemoryBenchmark',
              return_value='<out-file>')
  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  def testLiveRun(self, print_run_info, run_benchmark):
    self.wpr_updater.LiveRun()
    run_benchmark.assert_called_once_with(log_name='live', live=True)
    print_run_info.assert_called_once_with('<out-file>', chrome_log_file=True)

  @mock.patch('os.rename')
  def testRunBenchmark(self, rename):
    self._check_output.return_value = '  <chrome-log>'

    self.wpr_updater._RunSystemHealthMemoryBenchmark('<log_name>', True)

    # Check correct arguments when running benchmark.
    self._check_log.assert_called_once_with(
        [
          '.../run_benchmark', 'run', '--browser=system',
          'system_health.memory_desktop', '--output-format=html',
          '--show-stdout', '--reset-results', '--story-filter=^\\<story\\>$',
          '--browser-logging-verbosity=verbose', '--pageset-repeat=1',
          '--output-dir', '/tmp/dir', '--also-run-disabled-tests',
          '--use-live-sites'
        ],
        env={'LC_ALL': 'en_US.UTF-8'},
        log_path='/tmp/dir/<log_name>_<tstamp>')

    # Check logs are correctly extracted.
    self.assertListEqual(rename.mock_calls, [
      mock.call(
        '/tmp/dir/results.html', '/tmp/dir/<log_name>_<tstamp>.results.html'),
      mock.call('<chrome-log>', '/tmp/dir/<log_name>_<tstamp>.chrome.log'),
    ])

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

  @mock.patch('os.remove')
  def testDeleteExistingWpr(self, os_remove):
    self._open.return_value.__enter__.return_value.read.return_value = (
        '{"archives": {"<story>": {"DEFAULT": "<archive>"}}}')
    self.wpr_updater._DeleteExistingWpr()
    self.assertListEqual(os_remove.mock_calls, [
      mock.call('.../data/dir/<archive>'),
      mock.call('.../data/dir/<archive>.sha1'),
    ])

  @mock.patch('os.remove')
  def testDoesNotDeleteReusedWpr(self, os_remove):
    self._open.return_value.__enter__.return_value.read.return_value = (
        '{"archives": {"<story>": {"DEFAULT": "<archive>"}, '
        '"<other>": {"DEFAULT": "foo", "linux": "<arhive>"}}}')
    self.wpr_updater._DeleteExistingWpr()
    os_remove.assert_not_called()

  @mock.patch(WPR_UPDATER + '_ExtractMissingURLsFromLog',
              return_value=['foo', 'bar'])
  @mock.patch(WPR_UPDATER + 'WprUpdater._ExistingWpr', return_value='<archive>')
  @mock.patch('py_utils.binary_manager.BinaryManager', autospec=True)
  def testAddMissingURLsToArchive(self, bmanager, existing_wpr, extract_mock):
    del existing_wpr   # Unused.
    bmanager.return_value.FetchPath.return_value = '<wpr-go-bin>'
    self.wpr_updater._AddMissingURLsToArchive('<replay-log>')
    extract_mock.assert_called_once_with('<replay-log>')
    self._check_call.assert_called_once_with(
        ['<wpr-go-bin>', 'add', '<archive>', 'foo', 'bar'])

  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  @mock.patch(WPR_UPDATER + 'WprUpdater._DeleteExistingWpr')
  def testRecordWprDesktop(self, delete_existing_wpr, print_run_info):
    del delete_existing_wpr  # Unused.
    self.wpr_updater.RecordWpr()
    self._check_log.assert_called_once_with([
      '.../record_wpr', '--story-filter=^\\<story\\>$',
      '--browser=system', 'desktop_system_health_story_set'
    ], env={'LC_ALL': 'en_US.UTF-8'}, log_path='/tmp/dir/record_<tstamp>')
    print_run_info.assert_called_once_with(
        '/tmp/dir/record_<tstamp>', chrome_log_file=True, results_details=False)

  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  @mock.patch(WPR_UPDATER + 'WprUpdater._DeleteExistingWpr')
  def testRecordWprMobile(self, delete_existing_wpr, print_run_info):
    del delete_existing_wpr  # Unused.
    self.wpr_updater.device_id = '<serial>'
    self.wpr_updater.RecordWpr()
    self._check_log.assert_called_once_with([
      '.../record_wpr', '--story-filter=^\\<story\\>$',
      '--browser=android-system-chrome', '--device=<serial>',
      'mobile_system_health_story_set'
    ], env={'LC_ALL': 'en_US.UTF-8'}, log_path='/tmp/dir/record_<tstamp>')
    print_run_info.assert_called_once_with(
        '/tmp/dir/record_<tstamp>', chrome_log_file=False,
        results_details=False)

  @mock.patch(WPR_UPDATER + '_PrintRunInfo')
  @mock.patch(WPR_UPDATER + 'WprUpdater._RunSystemHealthMemoryBenchmark',
              return_value='<out-file>')
  def testReplayWpr(self, run_benchmark, print_run_info):
    self.wpr_updater.ReplayWpr()
    run_benchmark.assert_called_once_with(log_name='replay', live=False)
    print_run_info.assert_called_once_with('<out-file>', chrome_log_file=True)

  @mock.patch(WPR_UPDATER + 'WprUpdater._ExistingWpr',
              return_value=('<archive>', False))
  def testUploadWPR(self, existing_wpr):
    del existing_wpr  # Unused.
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
        start_git_hash='HEAD',
        end_git_hash='HEAD',
        target='performance_test_suite',
        patch='<issue-url>',
        bug_id='',
        story='<story>',
        extra_test_args='--pageset-repeat=1',
        configuration='mac-10_12_laptop_low_end-perf',
        benchmark='system_health.common_desktop')
    self.assertEqual(new_job.call_count, 3)

  @mock.patch(WPR_UPDATER + 'WprUpdater._GetBranchIssueUrl',
              return_value='<issue-url>')
  @mock.patch('core.services.pinpoint_service.NewJob',
              side_effect=request.ServerError(
                  mock.Mock(), mock.Mock(status=500), ''))
  def testStartPinPointJobsMobileFail(self, new_job, get_branch_issue_url):
    del get_branch_issue_url  # Unused.
    self.wpr_updater.device_id = '<serial>'
    self.assertEqual(
        self.wpr_updater.StartPinpointJobs(['<config>']), ([], ['<config>']))
    new_job.assert_called_once_with(
        start_git_hash='HEAD',
        end_git_hash='HEAD',
        target='performance_test_suite',
        patch='<issue-url>',
        bug_id='',
        story='<story>',
        extra_test_args='--pageset-repeat=1',
        configuration='<config>',
        benchmark='system_health.common_mobile')


if __name__ == "__main__":
  unittest.main()
