#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs captured sites framework recording and tests.

  $ tools/captured_sites/control.py [command] [arguments]
Commands:
  chrome  Starts a Chrome instance with autofill hooks
  wpr     Starts a WPR server instance to record or replay
  run     Starts a test for a single site or "*" for all sites
  refresh Starts a test for a single site or "*" for all sites, and records new
          server prediction responses.

Use "captured_sites [command] -h" for more information about each command.',

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

from __future__ import print_function

import argparse
import json
import os
import signal
import subprocess
import sys
import time

from collections import namedtuple

# Checking for environment variables.
_HOME_DIR = os.environ['HOME']
_DEFAULT_USER_DATA_DIR = os.path.join(_HOME_DIR, 'data/userdir')
_DEFAULT_LOG_DATA_DIR = os.path.join(_HOME_DIR, 'data/local_test_results')
if 'CAPTURED_SITES_USER_DATA_DIR' in os.environ:
  _USER_DATA_DIR_PATH = os.environ['CAPTURED_SITES_USER_DATA_DIR']
else:
  _USER_DATA_DIR_PATH = _DEFAULT_USER_DATA_DIR
if 'CAPTURED_SITES_LOG_DATA_DIR' in os.environ:
  _LOG_DATA_DIR_PATH = os.environ['CAPTURED_SITES_LOG_DATA_DIR']
else:
  _LOG_DATA_DIR_PATH = _DEFAULT_LOG_DATA_DIR

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


class Command():
  def __init__(self, description, arg_builders, launch_method):
    self.description = description
    self.arg_builders = arg_builders
    self.launch_method = launch_method

  def build_and_execute(self, args):
    parser = argparse.ArgumentParser(description=self.description)
    for arg_builder in self.arg_builders:
      arg_builder(parser)
    found_args = parser.parse_known_args(args)
    self.launch_method(found_args[0], found_args[1])


def _add_chrome_args(parser):
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
  _add_cert_args(parser)


def _add_wpr_args(parser):
  parser.add_argument('subhead',
                      choices=['record', 'replay'],
                      help=('Whether to record new traffic to an archive, '
                            'or replay from an existing archive.'))
  _add_cert_args(parser)


def _add_run_args(parser):
  parser.add_argument('-r',
                      '--release',
                      dest='target',
                      action='store_const',
                      default='Default',
                      const='Release',
                      help='Run tests on Release build of chrome.')
  parser.add_argument('-s',
                      '--store-log',
                      dest='store_log',
                      action='store_true',
                      help='Store the log and output in _LOG_DATA_DIR_PATH.')
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


def _add_cert_args(parser):
  parser.add_argument(
      '-c',
      '--cert-type',
      dest='cert_type',
      action='store',
      default='all',
      choices=['ecdsa', 'rsa', 'all'],
      help='Define tls certificate type for the session. Defaults to `all`.')


def _add_shared_args(parser):
  parser.add_argument('-p',
                      '--print-only',
                      dest='print_only',
                      action='store_true',
                      help='Build the command and print it but do not execute.')


def _add_scenario_site_args(parser):
  parser.add_argument('scenario_dir',
                      nargs='?',
                      default='',
                      choices=[
                          'sign_in_pass', 'sign_up_pass', 'sign_up_fill',
                          'capture_update_pass', '*', ''
                      ],
                      help=('Only for password tests to designate the specific '
                            'test scenario. Use * to indicate all password test'
                            ' scenarios.'))
  parser.add_argument('site_name',
                      help=('The site name which should have a match in '
                            'testcases.json. Use * to indicate all enumerated '
                            'sites in that file.'))


def _parse_command_args(command_names):
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter)
  parser.usage = __doc__
  parser.add_argument('name', choices=command_names)
  parser.add_argument('args', nargs=argparse.REMAINDER)
  return parser.parse_args()


def _make_process_call(command_args, print_only):
  command_text = ' '.join(command_args)
  print(command_text)
  if print_only:
    return

  if not os.path.exists(command_args[0]):
    raise EnvironmentError('Cannot locate binary to execute. '
                           'Ensure that working directory is chromium/src')
  subprocess.call(command_text, shell=True)


def _print_starting_url(url):
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
  print(data['startingURL'])
  print('')


def _retrieve_cert_info(cert_type):
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


def _launch_chrome(options, forward_args):
  if options.start_url:
    _print_starting_url(options.start_url)

  if not os.path.isdir(_USER_DATA_DIR_PATH):
    print('Required CAPTURED_SITES_USER_DATA_DIR "%s" cannot be found' %
          _USER_DATA_DIR_PATH)
    raise ValueError('Must set environment variable $CAPTURED_SITES_USER_DATA_D'
                     'IR or ensure default _USER_DATA_DIR_PATH exists')

  _, _, ignore_cert_list_arg = _retrieve_cert_info(options.cert_type)
  command_args = [
      options.build_target, ignore_cert_list_arg,
      '--user-data-dir="%s"' % _USER_DATA_DIR_PATH,
      '--disable-application-cache', '--show-autofill-signatures',
      '--enable-features=AutofillShowTypePredictions',
      '--disable-features=AutofillCacheQueryResponses'
  ]
  if options.wpr_selection:
    command_args.append(_HOOK_CHROME_TO_WPR)
  _make_process_call(command_args + forward_args, options.print_only)


def _launch_wpr(options, forward_args):
  command_args = [
      'third_party/catapult/telemetry/telemetry/bin/linux/x86_64/wpr',
      options.subhead, '--http_port=8080', '--https_port=8081'
  ]
  cert_arg, key_arg, _ = _retrieve_cert_info(options.cert_type)
  command_args.append(cert_arg)
  command_args.append(key_arg)

  command_args.append(_WPR_INJECT_SCRIPTS)
  if options.subhead == 'replay':
    command_args.append('--serve_response_in_chronological_sequence')

  if options.scenario_dir == '':
    wpr_path = f'{_AUTOFILL_ARTIFACTS_PATH}/{options.site_name}.wpr'
  else:
    wpr_path = f'{_PASSWORD_ARTIFACTS_PATH}/'\
             + f'{options.scenario_dir}/{options.site_name}.wpr'

  command_args.append(wpr_path)

  _make_process_call(command_args + forward_args, options.print_only)


def _launch_refresh(options, forward_args):
  _launch_test(options, forward_args, _AUTOFILL_REFRESH,
               _PASSWORD_MANAGER_REFRESH)


def _launch_run(options, forward_args):
  _launch_test(options, forward_args, _AUTOFILL_TEST, _PASSWORD_MANAGER_TEST)


def _launch_test(options, forward_args, gtest_filter_autofill,
                 gtest_filter_password):
  gtest_filter = gtest_filter_autofill
  gtest_parameter = options.site_name
  vmodule_name = _VMODULE_AUTOFILL_FILE
  if options.scenario_dir != '':
    gtest_filter = gtest_filter_password
    gtest_parameter = '%s_%s' % (options.scenario_dir, options.site_name)
    vmodule_name = _VMODULE_PASSWORD_FILE

  command_args = [
      'out/%s/captured_sites_interactive_tests' % options.target,
      '--gtest_filter="%s.Recipe/%s"' % (gtest_filter, gtest_parameter),
      '--test-launcher-interactive', '--enable-pixel-output-in-tests',
      '--vmodule=captured_sites_test_utils=2,%s,%s=1' %
      (options.verbose_logging, vmodule_name)
  ]

  if options.background:
    command_args.insert(0, _RUN_BACKGROUND)

  if options.add_disabled:
    command_args.append(_RUN_DISABLED_TESTS)

  if options.add_break_on_failure:
    command_args.append(_RUN_DEBUGGING_TESTS)

  if options.wpr_verbose:
    command_args.append('--wpr_verbose')

  if options.retry_count > 0:
    command_args.append('--test-launcher-retry-limit=%d' % options.retry_count)

  if options.autofill_cache_type:
    full_cache_type = _AUTOFILL_CACHE_TYPE_LOOKUP[options.autofill_cache_type]
    command_args.append('--autofill-server-type=%s ' % full_cache_type)

  if options.command_file:
    command_args.append('--command_file=%s' %
                        os.path.expanduser(options.command_file))

  if options.store_log:
    if not os.path.isdir(_LOG_DATA_DIR_PATH):
      print('Required LOG_DATA_DIR "%s" cannot be found' % _LOG_DATA_DIR_PATH)
      raise ValueError('Must set environment variable $LOG_DATA_DIR or '
                       'ensure default _LOG_DATA_DIR_PATH exists')
    logging_scenario_site_param = gtest_parameter.replace('*', 'all')
    command_args.append(
        '--test-launcher-summary-output={}/{}_output.json'.format(
            _LOG_DATA_DIR_PATH, logging_scenario_site_param))
    command_args.extend(forward_args)
    command_args.append('2>&1 | tee {}/{}_capture.log'.format(
        _LOG_DATA_DIR_PATH, logging_scenario_site_param))

  _make_process_call(command_args, options.print_only)


def _handle_signal(sig, _):
  """Handles received signals to make sure spawned test process are killed.

  sig (int): An integer representing the received signal, for example SIGTERM.
  """

  # Don't do any cleanup here, instead, leave it to the finally blocks.
  # Assumption is based on https://docs.python.org/3/library/sys.html#sys.exit:
  # cleanup actions specified by finally clauses of try statements are honored.

  # https://tldp.org/LDP/abs/html/exitcodes.html:
  # Exit code 128+n -> Fatal error signal "n".
  print('Signal to quit received, waiting for potential WPR write to complete')
  time.sleep(1)
  sys.exit(128 + sig)


def main():
  for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, _handle_signal)

  all_commands = {
      'chrome':
      Command('Start a Chrome instance with autofill hooks.',
              [_add_chrome_args, _add_shared_args], _launch_chrome),
      'wpr':
      Command('Start WPR to replay or record.',
              [_add_wpr_args, _add_shared_args, _add_scenario_site_args],
              _launch_wpr),
      'refresh':
      Command('Refresh the Server Predictions of an autofill or password test.',
              [_add_run_args, _add_shared_args, _add_scenario_site_args],
              _launch_refresh),
      'run':
      Command('Start an autofill or password test run.',
              [_add_run_args, _add_shared_args, _add_scenario_site_args],
              _launch_run)
  }
  options = _parse_command_args(all_commands.keys())
  command = all_commands[options.name]
  command.build_and_execute(options.args)


if __name__ == '__main__':
  sys.exit(main())
