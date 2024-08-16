# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to automate updating existing WPR benchmarks from live versions of the
# sites. Only supported on Mac/Linux.

from __future__ import print_function

import argparse
import datetime
import json
import os
import pathlib
import random
import re
import shutil
import subprocess
import tempfile
import time
import sys
import webbrowser

from chrome_telemetry_build import chromium_config
from core import cli_helpers
from core import path_util
from core.services import luci_auth
from core.services import pinpoint_service
from core.services import request

path_util.AddTelemetryToPath()
from telemetry import record_wpr
from telemetry.wpr import archive_info
from telemetry.internal.browser import browser_finder
from telemetry.internal.browser import browser_options
from telemetry.internal.util import binary_manager as telemetry_binary_manager

import py_utils
from py_utils import binary_manager, cloud_storage


SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
RESULTS2JSON = os.path.join(
    SRC_ROOT, 'third_party', 'catapult', 'tracing', 'bin', 'results2json')
HISTOGRAM2CSV = os.path.join(
    SRC_ROOT, 'third_party', 'catapult', 'tracing', 'bin', 'histograms2csv')
RUN_BENCHMARK = os.path.join(SRC_ROOT, 'tools', 'perf', 'run_benchmark')
DATA_DIR = os.path.join(SRC_ROOT, 'tools', 'perf', 'page_sets', 'data')
RECORD_WPR = os.path.join(SRC_ROOT, 'tools', 'perf', 'record_wpr')
DEFAULT_REVIEWERS = ['johnchen@chromium.org']
MISSING_RESOURCE_RE = re.compile(
    r'\[network\]: Failed to load resource: the server responded with a status '
    r'of 404 \(\) ([^\s]+)')
TELEMETRY_BIN_DEPS_CONFIG = os.path.join(
    path_util.GetTelemetryDir(), 'telemetry', 'binary_dependencies.json')
PY_EXECUTABLE = [sys.executable]


def _GetBranchName():
  branch_name = subprocess.check_output(
      ['git', 'rev-parse', '--abbrev-ref', 'HEAD']).strip()
  if isinstance(branch_name, bytes):
    return branch_name.decode("utf-8")
  return branch_name


def _OpenBrowser(url):
  # Redirect I/O before invoking browser to avoid it spamming our output.
  # Based on https://stackoverflow.com/a/2323563.
  savout = os.dup(1)
  saverr = os.dup(2)
  os.close(1)
  os.close(2)
  os.open(os.devnull, os.O_RDWR)
  try:
    webbrowser.open(url)
  finally:
    os.dup2(savout, 1)
    os.dup2(saverr, 2)


def _SendCLForReview(comment):
  subprocess.check_call(
      ['git', 'cl', 'comments', '--publish', '--add-comment', comment])


def _EnsureEditor():
  if 'EDITOR' not in os.environ:
    os.environ['EDITOR'] = cli_helpers.Prompt(
        'Looks like EDITOR environment varible is not defined. Please enter '
        'the command to view logs: ')


def _OpenEditor(filepath):
  subprocess.check_call([os.environ['EDITOR'], filepath])


def _PrepareEnv():
  # Enforce the same local settings for recording and replays on the bots.
  env = os.environ.copy()
  env['LC_ALL'] = 'en_US.UTF-8'
  return env


def _ExtractLogFile(out_file):
  # This method extracts the name of the chrome log file from the
  # run_benchmark output log and copies it to the temporary directory next to
  # the log file, which ensures that it is not overridden by the next run.
  try:
    line = subprocess.check_output(
      ['grep', 'Chrome log file will be saved in', out_file])
    os.rename(line.split()[-1], out_file + '.chrome.log')
  except subprocess.CalledProcessError as e:
    cli_helpers.Error('Could not find log file: {error}', error=e)


def _PrintResultsHTMLInfo(out_file):
  results_file = out_file + '.results.html'
  histogram_json = out_file + '.hist.json'
  histogram_csv = out_file + '.hist.csv'

  cli_helpers.Run(
      [RESULTS2JSON, results_file, histogram_json], env=_PrepareEnv())
  cli_helpers.Run(
      [HISTOGRAM2CSV, histogram_json, histogram_csv], env=_PrepareEnv())

  cli_helpers.Info('Metrics results: file://{path}', path=results_file)
  names = set([
      'console:error:network',
      'console:error:js',
      'console:error:all',
      'console:error:security'])
  with open(histogram_csv) as f:
    for line in f.readlines():
      line = line.split(',')
      if line[0] in names:
        cli_helpers.Info('    %-26s%s' % ('[%s]:' % line[0], line[2]))


def _ExtractMissingURLsFromLog(replay_log):
  missing_urls = []
  with open(replay_log) as replay_file:
    for line in replay_file:
      match = MISSING_RESOURCE_RE.search(line)
      if match:
        missing_urls.append(match.group(1))
  return missing_urls


def _CountLogLines(log_file, line_matcher_re=r'.*'):
  num_lines = 0
  line_matcher = re.compile(line_matcher_re)
  with open(log_file) as log_file_handle:
    for line in log_file_handle:
      if line_matcher.search(line):
        num_lines += 1
  return num_lines


def _UploadArchiveToGoogleStorage(archive):
  """Uploads specified WPR archive to the GS."""
  cli_helpers.Run([
    'upload_to_google_storage.py', '--bucket=chrome-partner-telemetry',
    archive])


def _GitAddArtifactHash(archive):
  """Stages changes into SHA1 file for commit."""
  archive_sha1 = archive + '.sha1'
  if not os.path.exists(archive_sha1):
    cli_helpers.Error(
        'Could not find upload artifact: {sha}', sha=archive_sha1)
    return False
  cli_helpers.Run(['git', 'add', archive_sha1])
  return True


def _PrintRunInfo(out_file, chrome_log_file=False, results_details=True):
  try:
    if results_details:
      _PrintResultsHTMLInfo(out_file)
  except Exception as e:
    cli_helpers.Error('Could not print results.html tests: %s' % e)

  cli_helpers.Info('Stdout/Stderr Log: %s' % out_file)
  if chrome_log_file:
    cli_helpers.Info('Chrome Log: %s.chrome.log' % out_file)
  cli_helpers.Info('    Total output:   %d' % _CountLogLines(out_file))
  cli_helpers.Info('    Total Console:  %d' %
                   _CountLogLines(out_file, r'DevTools console'))
  cli_helpers.Info('    [security]:     %d' %
                   _CountLogLines(out_file, r'DevTools console .security.'))
  cli_helpers.Info('    [network]:      %d' %
                   _CountLogLines(out_file, r'DevTools console .network.'))

  chrome_log = '%s.chrome.log' % out_file
  if os.path.isfile(chrome_log):
    cli_helpers.Info('    [javascript]:      %d' %
                     _CountLogLines(chrome_log, r'Uncaught .*Error'))

  if results_details:
    missing_urls = _ExtractMissingURLsFromLog(out_file)
    if missing_urls:
      cli_helpers.Info( 'Missing URLs in the archive:')
      for missing_url in missing_urls:
        cli_helpers.Info(' - %s' % missing_url)


class WprUpdater(object):
  def __init__(self, args):
    self._ValidateArguments(args)
    self.story = args.story
    self.bss = args.bss
    # TODO(sergiyb): Implement a method that auto-detects a single connected
    # device when device_id is set to 'auto'. This should take advantage of the
    # method adb_wrapper.Devices in catapult repo.
    self.device_id = args.device_id
    self.repeat = args.repeat
    self.binary = args.binary
    self.output_dir = tempfile.mkdtemp()
    self.bug_id = args.bug_id
    self.reviewers = args.reviewers or DEFAULT_REVIEWERS
    self.wpr_go_bin = None

    self._LoadArchiveInfo()

  def _ValidateArguments(self, args):
    if not args.story:
      raise ValueError('--story is required!')

  def _LoadArchiveInfo(self):
    config = chromium_config.GetDefaultChromiumConfig()
    tmp_recorder = record_wpr.WprRecorder(config, self.bss, parse_flags=False)
    self.wpr_archive_info = tmp_recorder.story_set.wpr_archive_info

  def _CheckLog(self, command, log_name):
    # This is a wrapper around cli_helpers.CheckLog that adds timestamp to the
    # log filename and substitutes placeholders such as {src}, {story},
    # {device_id} in the command.
    story_regex = '^%s$' % re.escape(self.story)
    command = [
        c.format(src=SRC_ROOT, story=story_regex, device_id=self.device_id)
        for c in command]
    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
    log_path = os.path.join(self.output_dir, '%s_%s' % (log_name, timestamp))
    cli_helpers.CheckLog(command, log_path=log_path, env=_PrepareEnv())
    return log_path

  def _IsDesktop(self):
    return self.device_id is None

  def _GetAllWprArchives(self):
    return self.wpr_archive_info.data['archives']

  def _GetWprArchivesForStory(self):
    archives = self._GetAllWprArchives()
    return archives.get(self.story)

  def _GetWprArchivePathsAndUsageForStory(self):
    """Parses JSON story config to extract info about WPR archive

    Returns:
      A list of 2-tuple with path to the current WPR archive for specified
      story and whether it is used by other benchmarks too.
    """
    archives = self._GetAllWprArchives()
    archive = self._GetWprArchivesForStory()
    if archive is None:
      return []

    existing = []
    for a in archive.values():
      used_in_other_stories = any(
          a in config.values() for story, config in archives.items()
          if story != self.story)
      existing.append((os.path.join(DATA_DIR, a), used_in_other_stories))
    return existing

  def _DeleteExistingWpr(self):
    """Deletes current WPR archive."""
    archive = self._GetWprArchivesForStory()
    if archive is None:
      return

    ans = cli_helpers.Ask(
          'For this story, should I erase all existing recordings that '
          'aren\'t used by other stories? Select yes if this is your '
          'first time re-recording this story.',
          ['yes', 'no'], default='no')
    if ans == 'no':
      return

    for archive, used_in_other_stories in self._GetWprArchivePathsAndUsageForStory():
      if used_in_other_stories:
        continue

      cli_helpers.Info('Deleting WPR: {archive}', archive=archive)
      if os.path.exists(archive):
        os.remove(archive)
      archive_sha1 = archive + '.sha1'
      if os.path.exists(archive_sha1):
        os.remove(archive_sha1)

    self.wpr_archive_info.RemoveStory(self.story)

  def _ExtractResultsFile(self, out_file):
    results_file = out_file + '.results.html'
    os.rename(os.path.join(self.output_dir, 'results.html'), results_file)

  def _BrowserArgs(self):
    """Generates args to be passed to RUN_BENCHMARK and UPDATE_WPR scripts."""
    if self.binary:
      return [
        '--browser-executable=%s' % self.binary,
        '--browser=exact',
      ]
    if self._IsDesktop():
      return ['--browser=system']
    return ['--browser=android-system-chrome']

  def _RunBenchmark(self, log_name, live=False):
    args = [RUN_BENCHMARK, 'run'] + self._BrowserArgs()

    benchmark = self.bss
    if self.bss == 'desktop_system_health_story_set':
      benchmark = 'system_health.memory_desktop'
    elif self.bss == 'mobile_system_health_story_set':
      benchmark = 'system_health.memory_mobile'
    args.append(benchmark)

    if not self._IsDesktop():
      args.append('--device={device_id}')

    args.extend([
        '--output-format=html', '--show-stdout', '--reset-results',
        '--story-filter={story}', '--browser-logging-verbosity=verbose',
        '--pageset-repeat=%s' % self.repeat, '--output-dir', self.output_dir,
        '--also-run-disabled-tests', '--legacy-json-trace-format'
    ])
    if live:
      args.append('--use-live-sites')
    out_file = self._CheckLog(args, log_name=log_name)
    self._ExtractResultsFile(out_file)
    if self._IsDesktop():  # Mobile test runner does not product the log file.
      _ExtractLogFile(out_file)
    return out_file

  def _GetBranchIssueUrl(self):
    output_file = os.path.join(self.output_dir, 'git_cl_issue.json')
    subprocess.check_output(['git', 'cl', 'issue', '--json', output_file])
    with open(output_file, 'r') as output_fd:
      return json.load(output_fd)['issue_url']

  def _SanitizedBranchPrefix(self):
    return 'update-wpr-%s' % re.sub(r'[^A-Za-z0-9-_.]', r'-', self.story)

  def _CreateBranch(self):
    new_branch_name = '%s-%d' % (
        self._SanitizedBranchPrefix(), random.randint(0, 10000))
    cli_helpers.Run(['git', 'new-branch', new_branch_name])

  def _FilterLogForDiff(self, log_filename):
    """Removes unimportant details from console logs for cleaner diffs.

    For example, log line from file `log_filename`

      2018-02-01 22:23:22,123 operation abcdef01-abcd-abcd-0123-abcdef012345
      from /tmp/tmpX34v/results.html took 22145ms when accessed via
      https://127.0.0.1:1233/endpoint

    would become

      <timestamp> operation <guid> from /tmp/tmp<random>/results.html took
      <duration> when accessed via https://127.0.0.1:<port>/endpoint

    Returns:
      Path to the filtered log.
    """
    with open(log_filename) as src, tempfile.NamedTemporaryFile(
        suffix='diff', dir=self.output_dir, delete=False) as dest:
      for line in src:
        # Remove timestamps.
        line = re.sub(
            r'\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3}', r'<timestamp>', line)
        # Remove GUIDs.
        line = re.sub(
            r'[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}', r'<guid>', line)
        # Remove random letters in paths to temp dirs and files.
        line = re.sub(r'(/tmp/tmp)[^/\s]+', r'\1<random>', line)
        # Remove random port in localhost URLs.
        line = re.sub(r'(://127.0.0.1:)\d+', r'\1<port>', line)
        # Remove random durations in ms.
        line = re.sub(r'\d+ ms', r'<duration>', line)
        dest.write(line)
        return dest.name

  def _GetTargetFromConfiguration(self, configuration):
    """Returns the target that should be used for a Pinpoint job."""
    if configuration == 'android-pixel6-perf':
      return 'performance_test_suite_android_clank_trichrome_chrome_google_64_32_bundle'
    if configuration in ('linux-perf', 'win-10-perf',
                         'mac-10_12_laptop_low_end-perf'):
      return 'performance_test_suite'
    raise RuntimeError('Unknown configuration %s' % configuration)

  def _StartPinpointJob(self, configuration):
    """Creates, starts a Pinpoint job and returns its URL."""
    try:
      resp = pinpoint_service.NewJob(
          base_git_hash='HEAD',
          target=self._GetTargetFromConfiguration(configuration),
          patch=self._GetBranchIssueUrl(),
          bug_id=self.bug_id or '',
          story=self.story,
          extra_test_args='--pageset-repeat=%d' % self.repeat,
          configuration=configuration,
          benchmark='system_health.common_%s' %
          ('desktop' if self._IsDesktop() else 'mobile'))
    except request.RequestError as e:
      cli_helpers.Comment(
          'Failed to start a Pinpoint job for {config} automatically:\n {err}',
          config=configuration, err=e.content)
      return None

    cli_helpers.Info(
        'Started a Pinpoint job for {configuration} at {url}',
        configuration=configuration, url=resp['jobUrl'])
    return resp['jobUrl']

  def _AddMissingURLsToArchive(self, replay_out_file):
    existing_wprs = self._GetWprArchivePathsAndUsageForStory()
    if len(existing_wprs) == 0:
      return

    if len(existing_wprs) == 1:
      archive = existing_wprs[0][0]
    else:
      cli_helpers.Comment("WPR Archives for this story:")
      print(str(self._GetWprArchivesForStory()))
      archive = cli_helpers.Ask(
          'Which archive should I add URLs to?',
          [e[0] for e in existing_wprs])

    missing_urls = _ExtractMissingURLsFromLog(replay_out_file)
    if not missing_urls:
      return

    if not self.wpr_go_bin:
      self.wpr_go_bin = (
        binary_manager.BinaryManager([TELEMETRY_BIN_DEPS_CONFIG]).FetchPath(
        'wpr_go', py_utils.GetHostArchName(), py_utils.GetHostOsName()))
    subprocess.check_call([self.wpr_go_bin, 'add', archive] + missing_urls)

  def LiveRun(self):
    cli_helpers.Step('LIVE RUN: %s' % self.story)
    out_file = self._RunBenchmark(
        log_name='live', live=True)
    _PrintRunInfo(out_file, chrome_log_file=self._IsDesktop())
    return out_file

  def Cleanup(self):
    if cli_helpers.Ask('Should I clean up the temp dir with logs?'):
      shutil.rmtree(self.output_dir, ignore_errors=True)
    else:
      cli_helpers.Comment(
        'No problem. All logs will remain in %s - feel free to remove that '
        'directory when done.' % self.output_dir)

  def RecordWpr(self):
    cli_helpers.Step('RECORD WPR: %s' % self.story)
    self._DeleteExistingWpr()
    args = ([RECORD_WPR, self.bss, '--story-filter={story}'] +
            self._BrowserArgs())
    if not self._IsDesktop():
      args.append('--device={device_id}')
    out_file = self._CheckLog(args, log_name='record')
    _PrintRunInfo(
        out_file, chrome_log_file=self._IsDesktop(), results_details=False)
    self._LoadArchiveInfo() # record_wpr overwrote this file

  def ReplayWpr(self):
    cli_helpers.Step('REPLAY WPR: %s' % self.story)
    out_file = self._RunBenchmark(
        log_name='replay', live=False)
    _PrintRunInfo(out_file, chrome_log_file=self._IsDesktop())
    return out_file

  def UploadWpr(self):
    # Attempts to upload all archives used by a story.
    # Note, if GoogleStorage already has the latest version
    # of a story, upload will be skipped.

    cli_helpers.Step('UPLOAD WPR: %s' % self.story)
    archives = self._GetWprArchivePathsAndUsageForStory()
    for archive in {a[0] for a in archives}:
      if not os.path.exists(archive):
        continue

      _UploadArchiveToGoogleStorage(archive)
      if not _GitAddArtifactHash(archive):
        return False
    return True

  def UploadCL(self, short_description=False):
    cli_helpers.Step('UPLOAD CL: %s' % self.story)
    if short_description:
      commit_message = 'Automated upload'
    else:
      commit_message = (
          'Add %s system health story\n\nThis CL was created automatically '
          'with tools/perf/update_wpr script' % self.story)
    if self.bug_id:
      commit_message += '\n\nBug: %s' % self.bug_id
    if subprocess.call(['git', 'diff', '--quiet']):
      cli_helpers.Run(['git', 'commit', '-a', '-m', commit_message])
    commit_msg_file = os.path.join(self.output_dir, 'commit_message.tmp')
    with open(commit_msg_file, 'w') as fd:
      fd.write(commit_message)
    return cli_helpers.Run([
      'git', 'cl', 'upload',
      '--reviewers', ','.join(self.reviewers),
      '--force',  # to prevent message editor from appearing
      '--message-file', commit_msg_file], ok_fail=True)

  def StartPinpointJobs(self, configs=None):
    job_urls = []
    failed_configs = []
    if not configs:
      if self._IsDesktop():
        configs = ['linux-perf', 'win-10-perf', 'mac-10_12_laptop_low_end-perf']
      else:
        configs = ['android-pixel6-perf']
    for config in configs:
      job_url = self._StartPinpointJob(config)
      if not job_url:
        failed_configs.append(config)
      else:
        job_urls.append(job_url)
    return job_urls, failed_configs

  def AutoRun(self):
    # Let the quest begin...
    cli_helpers.Comment(
        'This script will help you update the recording of a story. It will go '
        'through the following stages, which you can also invoke manually via '
        'subcommand specified in parentheses:')
    cli_helpers.Comment('  - help create a new branch if needed')
    cli_helpers.Comment('  - run story with live network connection (live)')
    cli_helpers.Comment('  - record story (record)')
    cli_helpers.Comment('  - replay the recording (replay)')
    cli_helpers.Comment('  - upload the recording to Google Storage (upload)')
    cli_helpers.Comment(
        '  - upload CL with updated recording reference (review)')
    cli_helpers.Comment('  - trigger pinpoint tryjobs (pinpoint)')
    cli_helpers.Comment('  - post links to these jobs on the CL')
    cli_helpers.Comment(
        'Note that you can always enter prefix of the answer to any of the '
        'questions asked below, e.g. "y" for "yes" or "j" for "just-replay".')

    # TODO(sergiyb): Detect if benchmark is not implemented and try to add it
    # automatically by copying the same benchmark without :<current-year> suffix
    # and changing name of the test, name of the benchmark and the year tag.

    # Create branch if needed.
    reuse_cl = False
    branch = _GetBranchName()
    if branch == 'HEAD':
      cli_helpers.Comment('You are not on a branch.')
      if not cli_helpers.Ask(
          'Should script create a new branch automatically?'):
        cli_helpers.Comment(
            'Please create a new branch and start this script again')
        return
      self._CreateBranch()
    else:
      issue = self._GetBranchIssueUrl()
      if issue is not None:
        issue_message = 'with an associated issue: %s' % issue
      else:
        issue_message = 'without an associated issue'
      cli_helpers.Comment(
          'You are on a branch {branch} {issue_message}. Please commit or '
          'stash any changes unrelated to the updated story before '
          'proceeding.', branch=branch, issue_message=issue_message)
      is_update_wpr_branch = re.match(
          r'%s-\d+' % self._SanitizedBranchPrefix(), branch)
      ans = cli_helpers.Ask(
          'Should the script create a new branch automatically, reuse '
          'existing one or exit?', answers=['create', 'reuse', 'exit'],
          default='reuse' if is_update_wpr_branch else 'create')
      if ans == 'create':
        self._CreateBranch()
      elif ans == 'reuse':
        reuse_cl = issue is not None
      elif ans == 'exit':
        return

    # Live run.
    live_out_file = self.LiveRun()
    cli_helpers.Comment(
        'Please inspect the live run results above for any errors.')
    ans = None
    while ans != 'continue':
      ans = cli_helpers.Ask(
          'Should I continue with recording, view metric results in a browser, '
          'view stdout/stderr output or stop?',
          ['continue', 'metrics', 'output', 'stop'], default='continue')
      if ans == 'stop':
        cli_helpers.Comment(
            'Please update the story class to resolve the observed issues and '
            'then run this script again.')
        return
      if ans == 'metrics':
        _OpenBrowser('file://%s.results.html' % live_out_file)
      elif ans == 'output':
        _OpenEditor(live_out_file)

    # Record & replay.
    action = 'record'
    replay_out_file = None
    while action != 'continue':
      if action == 'record':
        self.RecordWpr()
      if action == 'add-missing':
        self._AddMissingURLsToArchive(replay_out_file)
      if action in ['record', 'add-missing', 'just-replay']:
        replay_out_file = self.ReplayWpr()
        cli_helpers.Comment(
            'Check that the console:error:all metrics above have low values '
            'and are similar to the live run above.')
      if action == 'diff':
        diff_path = os.path.join(self.output_dir, 'live_replay.diff')
        with open(diff_path, 'w') as diff_file:
          subprocess.call([
            'diff', '--color', self._FilterLogForDiff(live_out_file),
            self._FilterLogForDiff(replay_out_file)], stdout=diff_file)
        _OpenEditor(diff_path)
      if action == 'stop':
        return
      action = cli_helpers.Ask(
          'Should I record and replay again, just replay, add all missing URLs '
          'into archive and try replay again, continue with uploading CL, stop '
          'and exit, or would you prefer to see diff between live/replay '
          'console logs?',
          ['record', 'just-replay', 'add-missing', 'continue', 'stop', 'diff'],
          default='continue')

    # Upload WPR and create a WIP CL for the new story.
    if not self.UploadWpr():
      return
    while self.UploadCL(short_description=reuse_cl) != 0:
      if not cli_helpers.Ask('Upload failed. Should I try again?'):
        return

    # Gerrit needs some time to sync its backends, hence we sleep here for 5
    # seconds. Otherwise, pinpoint app may get an answer that the CL that we've
    # just uploaded does not exist yet.
    cli_helpers.Comment(
        'Waiting 20 seconds for the Gerrit backends to sync, so that Pinpoint '
        'app can detect the newly-created CL.')
    time.sleep(20)

    # Trigger pinpoint jobs.
    configs_to_trigger = None
    job_urls = []
    while True:
      new_job_urls, configs_to_trigger = self.StartPinpointJobs(
          configs_to_trigger)
      job_urls.extend(new_job_urls)
      if not configs_to_trigger or not cli_helpers.Ask(
          'Do you want to try triggering the failed configs again?'):
        break

    if configs_to_trigger:
      if not cli_helpers.Ask(
          'Some jobs failed to trigger. Do you still want to send created '
          'CL for review?', default='no'):
        return

    # Post a link to the triggered jobs, publish CL for review and open it.
    _SendCLForReview(
        'Started the following Pinpoint jobs:\n%s' %
        '\n'.join('  - %s' % url for url in job_urls))
    cli_helpers.Comment(
        'Posted a message with Pinpoint job URLs on the CL and sent it for '
        'review. Opening the CL in a browser...')
    _OpenBrowser(self._GetBranchIssueUrl())

    # Hooray, you won! :-)
    cli_helpers.Comment(
        'Thank you, you have successfully updated the recording for %s. Please '
        'wait for LGTM and land the created CL.' % self.story)


class CrossbenchWprUpdater(object):
  """This class helps to update WPR archive files for the Crossbench tool.

  Currently it supports `android-trichrome-chrome-google-64-32-bundle` browser
  type only. The assumption is a single Android device is attached to the
  machine, and the device is connected to the network.
  """
  _CB_TOOL = os.path.join(SRC_ROOT, 'third_party', 'crossbench', 'cb.py')
  _BUCKET = cloud_storage.PARTNER_BUCKET
  _CHROME_BROWSER = '--browser=%s'
  _DEFAULT_BROWSER = 'android-trichrome-chrome-google-64-32-bundle'

  def __init__(self, args):
    self.story = args.story
    self.bss = args.bss
    self.device_id = args.device_id or 'adb'
    self.repeat = args.repeat
    self.binary = args.binary
    self.bug_id = args.bug_id
    self.reviewers = args.reviewers or DEFAULT_REVIEWERS
    self.wpr_go_bin = None
    self.cb_wprgo_file = args.cb_wprgo_file

    self._SetupOutput(args)
    self._LoadArchiveInfo()
    self._find_browser(self._DEFAULT_BROWSER)

  def _SetupOutput(self, args):
    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
    self.output_dir = os.path.join(args.output_dir or tempfile.mkdtemp(),
                                   timestamp)
    if os.path.exists(self.output_dir):
      raise FileExistsError(f'{self.output_dir} already exists!')
    pathlib.Path(self.output_dir).mkdir(parents=True)

  def _LoadArchiveInfo(self):
    self.wpr_archive_info = archive_info.WprArchiveInfo.FromFile(
        str(self.ArchiveFilePath(self.bss)), self._BUCKET)

  def _find_browser(self, browser_arg):
    options = browser_options.BrowserFinderOptions()
    options.chrome_root = pathlib.Path(SRC_ROOT)
    parser = options.CreateParser()
    telemetry_binary_manager.InitDependencyManager(None)
    parser.parse_args([self._CHROME_BROWSER % browser_arg])
    # Finding the browser package and installing the required dependencies.
    possible_browser = browser_finder.FindBrowser(options)
    if not possible_browser:
      raise ValueError(f'Unable to find Chrome browser of type: {browser_arg}')
    self.browser = possible_browser.settings.package

  def AutoRun(self):
    story_msg = f'the {self.story} in ' if self.story else ''
    if not cli_helpers.Ask(
        f'This script generates archive file for {story_msg} the '
        f'{self.bss} benchmark. It will generate a commit in your current '
        'branch. If you need to create a new branch or have uncommitted '
        'changes, please stop the script and create a fresh branch. Do '
        'you want to continue?',
        default='no'):
      return
    cb_wprgo = self.RecordWpr()
    self.ReplayWpr(cb_wprgo)
    if not cli_helpers.Ask(
        f'The {cb_wprgo} file has been generated and replayed. Please '
        f'see the Crossbench log file in {self.output_dir}. Are you sure '
        'to upload the new archive file to the cloud?',
        default='no'):
      return
    if not self.UploadWpr(cb_wprgo):
      cli_helpers.Error(f'Unabled to upload {cb_wprgo} to the cloud!')
      return
    cli_helpers.Comment(
        'Thank you, you have successfully updated the archive file. Please '
        'run `git status` to review the chagnes, upload the CL and send it to '
        f'{self.reviewers} for reviewing.')

  def LiveRun(self):
    cb_output_dir = os.path.join(self.output_dir, 'cb_live')
    command = self._GenerateCommandList([], cb_output_dir)
    self._CheckLog(command, log_name='live')

  def RecordWpr(self):
    cli_helpers.Step(f'RECORD WPR: {self.bss}')
    cb_output_dir = os.path.join(self.output_dir, 'cb_record')
    command = self._GenerateCommandList(['--probe=wpr'], cb_output_dir)
    self._CheckLog(command, log_name='record')
    cb_wprgo = os.path.join(cb_output_dir, 'archive.wprgo')
    cli_helpers.Info(f'WPRGO acrhive file: {cb_wprgo}')
    return cb_wprgo

  def ReplayWpr(self, cb_wprgo=None):
    cb_wprgo = cb_wprgo or self.cb_wprgo_file
    if not os.path.exists(cb_wprgo):
      raise FileNotFoundError(f'{cb_wprgo} not found!')
    cb_output_dir = os.path.join(self.output_dir, 'cb_replay')
    cli_helpers.Step(f'REPLAY WPR: {cb_wprgo}')
    network = [f'--network={{type:"wpr", path:"{cb_wprgo}"}}']
    command = self._GenerateCommandList(network, cb_output_dir)
    self._CheckLog(command, log_name='replay')

  def UploadWpr(self, cb_wprgo=None):
    cb_wprgo = cb_wprgo or self.cb_wprgo_file
    if not os.path.exists(cb_wprgo):
      raise FileNotFoundError(f'{cb_wprgo} not found!')
    cli_helpers.Step(f'UPLOAD WPR: {cb_wprgo}')
    archive = self._GetDataWprArchivePath()
    self._CopyTempWprgoToData(cb_wprgo, archive)
    _UploadArchiveToGoogleStorage(archive)
    return _GitAddArtifactHash(archive)

  def UploadCL(self):
    raise NotImplementedError()

  def StartPinpointJobs(self):
    raise NotImplementedError()

  def Cleanup(self):
    pass

  @staticmethod
  def ArchiveFilePath(benchmark_name):
    return os.path.join(DATA_DIR, f'crossbench_android_{benchmark_name}.json')

  def _CheckLog(self, command, log_name):
    log_path = os.path.join(self.output_dir, f'{log_name}.log')
    cli_helpers.CheckLog(command, log_path=log_path, env=_PrepareEnv())
    cli_helpers.Info(f'Stdout/Stderr Log: {log_path}')
    return log_path

  def _GenerateCommandList(self, args=None, cb_output_dir=None):
    args = args or []
    cb_output_dir = cb_output_dir or self.output_dir
    command = PY_EXECUTABLE + [
        f'{self._CB_TOOL}',
        self.bss,
        '--repeat=1',
        f'--browser={self.device_id}:{self.browser}',
        '--verbose',
        '--debug',
        '--no-symlinks',
        f'--out-dir={cb_output_dir}',
    ] + args
    if self.story:
      command += [f'--story={self.story}']
    return command

  def _GetDataWprArchivePath(self):
    """Gets the data archive file name by parsing the JSON story config."""
    archives = self.wpr_archive_info.data['archives']
    archive = None
    if self.story:
      archive = archives.get(self.story)
    elif archives and len(archives.keys()) == 1:
      archive = next(iter(archives.values()))
    if not archive:
      raise ValueError('Either --story is required or it was not found!')
    archive_name = next(iter(archive.values()))
    return os.path.join(DATA_DIR, archive_name)

  def _CopyTempWprgoToData(self, src_path, des_path):
    """Copies the archive file to the `DATA_DIR` in the repository."""
    if os.path.exists(des_path):
      os.remove(des_path)
    shutil.copy2(src_path, des_path)


def Main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('-s',
                      '--story',
                      dest='story',
                      required=False,
                      help='Story to be recorded, replayed or uploaded. '
                      'If you are recording a system_health benchmark, '
                      'use desktop_system_health_story_set or '
                      'mobile_system_health_story_set')
  parser.add_argument(
      '-bss',
      '--benchmark-or-story-set',
      dest='bss',
      required=True,
      help='Benchmark or story set to be recorded, replayed or uploaded. '
      'If you are recording a system health story, use '
      'desktop_system_health_story_set or mobile_system_health_story_set.')
  parser.add_argument(
      '-d', '--device-id', dest='device_id',
      help='Specify the device serial number listed by `adb devices`. When not '
           'specified, the script runs in desktop mode.')
  parser.add_argument(
      '-b', '--bug', dest='bug_id',
      help='Bug ID to be referenced on created CL')
  parser.add_argument(
      '-r', '--reviewer', action='append', dest='reviewers',
      help='Email of the reviewer(s) for the created CL.')
  parser.add_argument(
      '--pageset-repeat', type=int, default=1, dest='repeat',
      help='Number of times to repeat the entire pageset.')
  parser.add_argument(
      '--binary', default=None,
      help='Path to the Chromium/Chrome binary relative to output directory. '
           'Defaults to default Chrome browser installed if not specified.')
  parser.add_argument('-cb',
                      '--crossbench',
                      action='store_true',
                      dest='is_cb',
                      default=False,
                      help='Whether to use the Crossbench tool.')
  parser.add_argument(
      '--out',
      '--out-dir',
      '--output-dir',
      dest='output_dir',
      default=None,
      help='Path to generate log and archive files for Crossbench tool. '
      'Defaults to generate a random folder in the system temp folder.')
  parser.add_argument(
      '--cb-wprgo',
      '--cb-wprgo-file',
      dest='cb_wprgo_file',
      default=None,
      help='Path to the target Crossbench WPRGO file.'
      'Defaults to generate `archive.wprgo` file in the output folder.')

  subparsers = parser.add_subparsers(
      title='Mode in which to run this script', dest='command')
  subparsers.add_parser(
      'auto', help='interactive mode automating updating a recording')
  subparsers.add_parser('live', help='run story on a live website')
  subparsers.add_parser('record', help='record story from a live website')
  subparsers.add_parser('replay', help='replay story from the recording')
  subparsers.add_parser('upload', help='upload recording to the Google Storage')
  subparsers.add_parser('review', help='create a CL with updated recording')
  subparsers.add_parser(
      'pinpoint', help='trigger Pinpoint jobs to test the recording')

  args = parser.parse_args(argv)

  if args.is_cb:
    updater = CrossbenchWprUpdater(args)
  else:
    updater = WprUpdater(args)
  try:
    if args.command == 'auto':
      _EnsureEditor()
      luci_auth.CheckLoggedIn()
      updater.AutoRun()
    elif args.command =='live':
      updater.LiveRun()
    elif args.command == 'record':
      updater.RecordWpr()
    elif args.command == 'replay':
      updater.ReplayWpr()
    elif args.command == 'upload':
      updater.UploadWpr()
    elif args.command == 'review':
      updater.UploadCL()
    elif args.command == 'pinpoint':
      luci_auth.CheckLoggedIn()
      updater.StartPinpointJobs()
  finally:
    updater.Cleanup()
