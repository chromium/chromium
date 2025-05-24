# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs captured sites framework recording and tests.

  $ tools/captured_sites/control.py [command] [arguments]
Commands:
  build   Builds a captured_sites_interactive_tests target
  chrome  Starts a Chrome instance with autofill hooks
  wpr     Starts a WPR server instance to record or replay
  run     Starts a test for a single site or "*" for all sites
  refresh Starts a test for a single site or "*" for all sites, and records new
          server prediction responses.

Use "[command] -h" for more information about each command.',

This script attempts to simplify the various configuration and override options
that are available in creating and executing the Captured Sites Framework for
Autofill and Password Manager.

This script assumes execution location is the src folder of the chromium
checkout. Commands should be run from chromium/src directory.

Also assumes that built targets are in :
  out/Default for is_debug = true
  out/Release for is_debug = false

Some environment variables should be set in order to use this script to its
full potential.

  CAPTURED_SITES_USER_DATA_DIR - a location to store local information about the
  chromium profile. This allows the tester to pull back the address and credit
  card profile information without restarting it each time.
  CAPTURED_SITES_LOG_DATA_DIR - a location to store log data for easier parsing
  after a test run has been completed.

Common tasks:
Recording a new test for site 'youtube' (requires two terminal windows):
  Window 1$: tools/captured_sites/control.py wpr record youtube
  Window 2$: tools/captured_sites/control.py chrome -w -r

Checking a recorded test for site 'youtube' (requires two terminal windows):
  Window 1$ tools/captured_sites/control.py wpr replay youtube
  Window 2$ tools/captured_sites/control.py chrome -w -r -u youtube

Running all 'sign_in_pass' tests and saving the logs:
  $ tools/captured_sites/control.py run -s sign_in_pass *

Running disabled autofill test 'rei':
  $ tools/captured_sites/control.py run -d rei

Running autofill test 'rei' with ability to pause at each step:
  $ tools/captured_sites/control.py run -q path/to/pipe rei

"""

import argparse
from collections import namedtuple
import json
import os
import subprocess
import sys

# Checking for environment variables.
_HOME_DIR = os.environ['HOME']
_DEFAULT_USER_DATA_DIR = os.path.join(_HOME_DIR, 'data/userdir')
_DEFAULT_LOG_DATA_DIR = os.path.join(_HOME_DIR, 'data/local_test_results')

# Long text chunks that will be used in command constructions.
_EXTRA_BROWSER_AUTOFILL = ('autofill_download_manager=1,form_cache=1,'
                           'autofill_agent=1,autofill_handler=1,'
                           'form_structure=1,cache_replayer=2')
_WPR_INJECT_SCRIPTS = ('--inject_scripts=third_party/catapult/web_page_replay_g'
                       'o/deterministic.js,chrome/test/data/web_page_replay_go_'
                       'helper_scripts/automation_helper.js')
_NORMAL_BROWSER_AUTOFILL = 'cache_replayer=1'
_RUN_BACKGROUND = 'testing/xvfb.py'
_RUN_DISABLED_TESTS = '--gtest_also_run_disabled_tests'
_RUN_DEBUGGING_TESTS = '--gtest_break_on_failure'

_AUTOFILL_ARTIFACTS_PATH = 'chrome/test/data/autofill/captured_sites/artifacts'
_AUTOFILL_TEST = '*/AutofillCapturedSitesInteractiveTest'
_AUTOFILL_REFRESH = '*/AutofillCapturedSitesRefresh'
_PASSWORD_ARTIFACTS_PATH = 'chrome/test/data/password/captured_sites/artifacts'
_PASSWORD_MANAGER_TEST = '*/CapturedSitesPasswordManagerBrowserTest'
_PASSWORD_MANAGER_REFRESH = '*/CapturedSitesPasswordManagerRefresh'
_VMODULE_AUTOFILL_FILE = 'autofill_captured_sites_interactive_uitest'
_VMODULE_PASSWORD_FILE = 'password_manager_captured_sites_interactive_uitest'

_WPR_SUPPORT_FILES_PATH = ('components/test/data/autofill/'
                           'web_page_replay_support_files')

_STABLE_GOOGLE_CHROME = '/usr/bin/google-chrome'
_RELEASE_BUILD_CHROME = 'out/Release/chrome'

_HOOK_CHROME_TO_WPR = ('--host-resolver-rules="MAP *:80 127.0.0.1:8080,'
                       'MAP *:443 127.0.0.1:8081,EXCLUDE localhost"')

_AUTOFILL_CACHE_TYPE_LOOKUP = {
    'SavedCache': 'SavedCache',
    'ProductionServer': 'ProductionServer',
    'OnlyLocalHeuristics': 'OnlyLocalHeuristics',
    'c': 'SavedCache',
    'p': 'ProductionServer',
    'n': 'OnlyLocalHeuristics'
}

WprInfo = namedtuple('WprInfo', ['cert', 'key', 'hash'])
_WPR_CERT_LOOKUP = {
    'ecdsa':
    WprInfo('ecdsa_cert.pem', 'ecdsa_key.pem',
            '2HcXCSKKJS0lEXLQEWhpHUfGuojiU0tiT5gOF9LP6IQ='),
    'rsa':
    WprInfo('wpr_cert.pem', 'wpr_key.pem',
            'PoNnQAwghMiLUPg1YNFtvTfGreNT8r9oeLEyzgNCJWc='),
}


def available_commands():
  commands = {
      'build': BuildCommand,
      'chrome': ChromeCommand,
      'refresh': RefreshCommand,
      'run': RunCommand,
      'wpr': WprCommand,
  }
  return commands


def parse_command():
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter)
  parser.usage = __doc__
  parser.add_argument('name', choices=available_commands().keys())
  parser.add_argument('args', nargs=argparse.REMAINDER)
  name_args = parser.parse_args()
  return name_args.name, name_args.args


def initiate_command(command_name, local_environ=os.environ.copy()):
  class_object = available_commands()[command_name]
  return class_object(local_environ)


def initiate_and_build_command():
  command_name, command_args = parse_command()
  instance = initiate_command(command_name)
  instance.build(command_args)
  return instance


class Command():

  def __init__(self, description, local_environ):
    self.description = description
    self.setup_environ(local_environ)

  def setup_environ(self, local_environ):
    self.local_environ = local_environ

    if 'CAPTURED_SITES_USER_DATA_DIR' in self.local_environ:
      self.user_data_dir_path = self.local_environ[
          'CAPTURED_SITES_USER_DATA_DIR']
    else:
      self.user_data_dir_path = _DEFAULT_USER_DATA_DIR

    if 'CAPTURED_SITES_LOG_DATA_DIR' in self.local_environ:
      self.log_data_dir_path = self.local_environ['CAPTURED_SITES_LOG_DATA_DIR']
    else:
      self.log_data_dir_path = _DEFAULT_LOG_DATA_DIR

  def build(self, args):
    parser = argparse.ArgumentParser(description=self.description)
    self.add_args(parser)
    parse_result = parser.parse_known_args(args)

    self.options = parse_result[0]
    self.command_args = []
    self.additional_args = parse_result[1]

  def print(self):
    command_text = ' '.join(self.command_args + self.additional_args)
    print(command_text)
    return command_text

  def launch(self):
    self.print()
    if self.options.print_only:
      return
    self.make_process_call(self.command_args + self.additional_args)

  def insert_cert_info(self, cert_type):
    cert_paths = []
    key_paths = []
    hashes = []
    for cert_name, cert_info in _WPR_CERT_LOOKUP.items():
      if cert_type == 'all' or cert_type == cert_name:
        cert_paths.append(f'{_WPR_SUPPORT_FILES_PATH}/{cert_info.cert}')
        key_paths.append(f'{_WPR_SUPPORT_FILES_PATH}/{cert_info.key}')
        hashes.append(cert_info.hash)
    cert_arg = '--https_cert_file=' + ','.join(cert_paths)
    self.command_args.append(cert_arg)
    key_arg = '--https_key_file=' + ','.join(key_paths)
    self.command_args.append(key_arg)

  def make_process_call(self, args):
    command_text = ' '.join(args)

    if not os.path.exists(args[0]):
      raise EnvironmentError('Cannot locate binary to execute. '
                             'Ensure that working directory is chromium/src')
    subprocess.call(command_text, shell=True)

  def add_args(self, parser):
    parser.add_argument(
        '-p',
        '--print-only',
        dest='print_only',
        action='store_true',
        help='Build the command and print it but do not execute.')

  def add_cert_args(self, parser):
    parser.add_argument(
        '-c',
        '--cert-type',
        dest='cert_type',
        action='store',
        default='all',
        choices=['ecdsa', 'rsa', 'all'],
        help='Define tls certificate type for the session. Defaults to `all`.')

  def add_scenario_site_args(self, parser):
    parser.add_argument(
        'scenario_dir',
        nargs='?',
        default='',
        choices=[
            'sign_in_pass', 'sign_up_pass', 'sign_up_fill',
            'capture_update_pass', '*', ''
        ],
        help=('Only for password tests to designate the specific '
              'test scenario. Use * to indicate all password test'
              ' scenarios.'))
    parser.add_argument(
        'site_name',
        help=('The site name which should have a match in '
              'testcases.json. Use * to indicate all enumerated '
              'sites in that file.'))

  def retrieve_cert_info(self, cert_type):
    cert_paths = []
    key_paths = []
    hashes = []
    for cert_name, cert_info in _WPR_CERT_LOOKUP.items():
      if cert_type == 'all' or cert_type == cert_name:
        cert_paths.append(f'{_WPR_SUPPORT_FILES_PATH}/{cert_info.cert}')
        key_paths.append(f'{_WPR_SUPPORT_FILES_PATH}/{cert_info.key}')
        hashes.append(cert_info.hash)
    cert_arg = '--https_cert_file=' + ','.join(cert_paths)
    key_arg = '--https_key_file=' + ','.join(key_paths)
    ignore_cert_list_arg = '--ignore-certificate-errors-spki-list=' + ','.join(
        hashes)
    return cert_arg, key_arg, ignore_cert_list_arg


class BuildCommand(Command):

  def __init__(self, local_environ):
    super().__init__('Build the test target.', local_environ)

  def build(self, args):
    super().build(args)
    self.command_args = [
        'autoninja', '-C', 'out/' + self.options.binary_folder,
        'captured_sites_interactive_tests'
    ]

  def make_process_call(self, args):
    command_text = ' '.join(args)
    subprocess.call(command_text, shell=True)

  def add_args(self, parser):
    super().add_args(parser)
    parser.add_argument(
        '-r',
        '--release',
        dest='binary_folder',
        default='Default',
        const='Release',
        help=
        'Start a build of captured_sites_interactive_tests in Release folder.',
        action='store_const')


class ChromeCommand(Command):

  def __init__(self, local_environ):
    super().__init__('Start a Chrome instance with autofill hooks.',
                     local_environ)

  def build(self, args):
    super().build(args)

    _, _, ignore_cert_list_arg = self.retrieve_cert_info(self.options.cert_type)

    self.command_args = [self.options.build_target, ignore_cert_list_arg]

    if not os.path.isdir(self.user_data_dir_path):
      print('Required CAPTURED_SITES_USER_DATA_DIR "%s" cannot be found' %
            self.user_data_dir_path)
      raise ValueError(
          'Must set environment variable $CAPTURED_SITES_USER_DATA_D'
          'IR or ensure default _DEFAULT_USER_DATA_DIR exists')
    else:
      self.command_args.append('--user-data-dir="%s"' % self.user_data_dir_path)

    self.command_args.extend([
        '--disable-application-cache', '--show-autofill-signatures',
        '--enable-features=AutofillShowTypePredictions',
        '--disable-features=AutofillCacheQueryResponses'
    ])

    if self.options.wpr_selection:
      self.command_args.append(_HOOK_CHROME_TO_WPR)

    if self.options.start_url:
      startURL = self._print_starting_url(self.options.start_url)
      if startURL:
        self.command_args.append(startURL)

  def add_args(self, parser):
    super().add_args(parser)
    parser.add_argument('-r',
                        '--release',
                        dest='build_target',
                        default=_STABLE_GOOGLE_CHROME,
                        const=_RELEASE_BUILD_CHROME,
                        help='Start Release build of chrome.',
                        action='store_const')
    parser.add_argument('-w',
                        '--wpr',
                        dest='wpr_selection',
                        action='store_true',
                        help='Point chrome instance at wpr service.')
    parser.add_argument('-u',
                        '--url',
                        dest='start_url',
                        action='store',
                        help='Grab starting URL from test recipe.')
    self.add_cert_args(parser)

  def _print_starting_url(self, url):
    base_path = _AUTOFILL_ARTIFACTS_PATH
    if '-' in url:
      base_path = _PASSWORD_ARTIFACTS_PATH
      url = url.replace('-', '/')
    archive_path = f'{base_path}/{url}.test'

    if not os.path.exists(archive_path):
      print('No file found for "%s"' % url, file=sys.stderr)
      return
    with open(archive_path, 'r') as read_file:
      data = json.load(read_file)
    if not 'startingURL' in data:
      print('No startingURL found in file for "%s"' % url, file=sys.stderr)
      return
    print('%s test starts at:' % url, file=sys.stderr)
    startURL = data['startingURL']
    print(startURL)
    print('')
    return startURL


class TestCommand(Command):

  def __init__(self, description, gtest_filter_autofill, gtest_filter_password,
               local_environ):
    super().__init__(description, local_environ)
    self.gtest_filter_autofill = gtest_filter_autofill
    self.gtest_filter_password = gtest_filter_password

  def extract_parameter(self):
    if self.options.scenario_dir != '':
      self.gtest_parameter = '%s_%s' % (self.options.scenario_dir,
                                        self.options.site_name)
      self.vmodule_name = _VMODULE_PASSWORD_FILE
      self.gtest_filter = self.gtest_filter_password
    else:
      self.gtest_parameter = self.options.site_name
      self.vmodule_name = _VMODULE_AUTOFILL_FILE
      self.gtest_filter = self.gtest_filter_autofill

  def build(self, args):
    super().build(args)

    self.extract_parameter()

    self.command_args = [
        'out/%s/captured_sites_interactive_tests' % self.options.target,
        '--gtest_filter="%s.Recipe/%s"' %
        (self.gtest_filter, self.gtest_parameter),
        '--enable-pixel-output-in-tests',
    ]

    if self.options.use_bot_timeout:
      self.command_args.extend([
          '--ui-test-action-max-timeout=180000',
          '--test-launcher-timeout=180000'
      ])
    else:
      self.command_args.append('--test-launcher-interactive')

    self.command_args.append('--vmodule=captured_sites_test_utils=2,%s,%s=1' %
                             (self.options.verbose_logging, self.vmodule_name))

    if self.options.background:
      self.command_args.insert(0, _RUN_BACKGROUND)

    if self.options.add_disabled:
      self.command_args.append(_RUN_DISABLED_TESTS)

    if self.options.add_break_on_failure:
      self.command_args.append(_RUN_DEBUGGING_TESTS)

    if self.options.wpr_verbose:
      self.command_args.append('--wpr_verbose')

    if self.options.retry_count > 0:
      self.command_args.append('--test-launcher-retry-limit=%d' %
                               self.options.retry_count)

    if self.options.autofill_cache_type:
      full_cache_type = _AUTOFILL_CACHE_TYPE_LOOKUP[
          self.options.autofill_cache_type]
      self.command_args.append('--autofill-server-type=%s ' % full_cache_type)

    if self.options.command_file:
      self.command_args.append('--command_file=%s' %
                               os.path.expanduser(self.options.command_file))

    if self.options.store_log:
      if not os.path.isdir(self.log_data_dir_path):
        print('Required LOG_DATA_DIR "%s" cannot be found' %
              self.log_data_dir_path)
        raise ValueError('Must set environment variable $LOG_DATA_DIR or '
                         'ensure default _DEFAULT_LOG_DATA_DIR exists')
      logging_scenario_site_param = self.gtest_parameter.replace('*', 'all')
      self.command_args.append(
          '--test-launcher-summary-output={}/{}_output.json'.format(
              self.log_data_dir_path, logging_scenario_site_param))
      self.command_args.extend(self.additional_args)
      self.additional_args = []
      self.command_args.append('2>&1 | tee {}/{}_capture.log'.format(
          self.log_data_dir_path, logging_scenario_site_param))

  def add_args(self, parser):
    super().add_args(parser)
    parser.add_argument('-r',
                        '--release',
                        dest='target',
                        action='store_const',
                        default='Default',
                        const='Release',
                        help='Run tests on Release build of chrome.')
    parser.add_argument(
        '-s',
        '--store-log',
        dest='store_log',
        action='store_true',
        help=f'Store the log and output in {self.log_data_dir_path}.')
    parser.add_argument('-b',
                        '--background',
                        dest='background',
                        action='store_true',
                        help='Run the test in background with xvfb.py.')
    parser.add_argument('-d',
                        '--disabled',
                        dest='add_disabled',
                        action='store_true',
                        help='Also run disabled tests that match the filter.')
    parser.add_argument(
        '-u',
        '--use_bot_timeout',
        dest='use_bot_timeout',
        action='store_true',
        help=('Use the same timeout settings as exists on the bot (3 minutes) '
              'instead of the `test-launcher-interactive` setting. This is '
              'particularly useful when bisecting.'))
    parser.add_argument('-f',
                        '--break_on_failure',
                        dest='add_break_on_failure',
                        action='store_true',
                        help=('Run tests in single-process mode and brings the '
                              'debugger on an assertion failure.'))
    parser.add_argument('-v',
                        '--verbose',
                        dest='verbose_logging',
                        action='store_const',
                        default=_NORMAL_BROWSER_AUTOFILL,
                        const=_EXTRA_BROWSER_AUTOFILL,
                        help='Log verbose Autofill Server information.')
    parser.add_argument('-t',
                        '--test-retry',
                        dest='retry_count',
                        action='store',
                        default=0,
                        type=int,
                        help='How many times to retry failed tests.')
    parser.add_argument('-a',
                        '--autofill-cache-type',
                        dest='autofill_cache_type',
                        choices=_AUTOFILL_CACHE_TYPE_LOOKUP.keys(),
                        action='store',
                        help='Control the autofill cache behavior.')
    parser.add_argument('-q',
                        '--command_file',
                        dest='command_file',
                        action='store',
                        default='',
                        type=str,
                        help='Location of "pipe: file')
    parser.add_argument('-w',
                        '--wpr_verbose',
                        dest='wpr_verbose',
                        action='store_true',
                        help='Also include verbose WPR output.')
    self.add_scenario_site_args(parser)


class RefreshCommand(TestCommand):

  def __init__(self, local_environ):
    super().__init__(
        'Refresh the Server Predictions of an autofill or password test.',
        _AUTOFILL_REFRESH, _PASSWORD_MANAGER_REFRESH, local_environ)


class RunCommand(TestCommand):

  def __init__(self, local_environ):
    super().__init__('Start an autofill or password test run.', _AUTOFILL_TEST,
                     _PASSWORD_MANAGER_TEST, local_environ)


class WprCommand(Command):

  def __init__(self, local_environ):
    super().__init__('Start WPR to replay or record.', local_environ)

  def build(self, args):
    super().build(args)
    self.command_args = [
        'third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr',
        self.options.subhead, '--http_port=8080', '--https_port=8081'
    ]

    self.command_args.append(_WPR_INJECT_SCRIPTS)

    if self.options.subhead == 'replay':
      self.command_args.append('--serve_response_in_chronological_sequence')

    self.insert_cert_info(self.options.cert_type)

    if self.options.scenario_dir == '':
      wpr_path = f'{_AUTOFILL_ARTIFACTS_PATH}/{self.options.site_name}.wpr'
    else:
      wpr_path = f'{_PASSWORD_ARTIFACTS_PATH}/'\
               + f'{self.options.scenario_dir}/{self.options.site_name}.wpr'
    self.command_args.append(wpr_path)

  def add_args(self, parser):
    parser.add_argument('subhead',
                        choices=['record', 'replay'],
                        help=('Whether to record new traffic to an archive, '
                              'or replay from an existing archive.'))
    self.add_cert_args(parser)
    self.add_scenario_site_args(parser)
    super().add_args(parser)
