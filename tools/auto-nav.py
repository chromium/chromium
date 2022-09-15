# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script runs Chrome and automatically navigates through the given list of
URLs the specified number of times.

Usage: vpython3 auto-nav.py <chrome dir> <number of navigations> <url> <url> ...

Optional flags:
* --interval <seconds>, -i <seconds>: specify a number of seconds to wait
                                      between navigations, e.g., -i=5
* --start_prompt, -s: start Chrome, then wait for the user to press Enter before
                      starting auto-navigation
* --exit-prompt, -e: after auto-navigation, wait for the user to press Enter
                     before shutting down chrome.exe
* --idlewakeups_dir: Windows only; specify the directory containing
                     idlewakeups.exe to print measurements taken by IdleWakeups,
                     e.g., --idlewakeups_dir=tools/win/IdleWakeups/x64/Debug

Optional flags to chrome.exe, example:
-- --user-data-dir=temp --disable-features=SomeFeature
Note: must be at end of command, following options terminator "--". The options
terminator stops command-line options from being interpreted as options for this
script, which would cause an unrecognized-argument error.
"""

# [VPYTHON:BEGIN]
# python_version: "3.8"
# wheel: <
#   name: "infra/python/wheels/selenium-py2_py3"
#   version: "version:3.14.0"
# >
# wheel: <
#   name: "infra/python/wheels/urllib3-py2_py3"
#   version: "version:1.24.3"
# >
# wheel: <
#   name: "infra/python/wheels/psutil/${vpython_platform}"
#   version: "version:5.7.2"
# >
# [VPYTHON:END]

import argparse
import os
import subprocess
import sys
import time
import urllib

try:
  import psutil
  from selenium import webdriver
except ImportError:
  print('Error importing required modules. Run with vpython3 instead of '
        'python.')
  sys.exit(1)

DEFAULT_INTERVAL = 1
EXIT_CODE_ERROR = 1

# Splits list |positional_args| into two lists: |urls| and |chrome_args|, where
# arguments starting with '-' are treated as chrome args, and the rest as URLs.
def ParsePositionalArgs(positional_args):
  urls, chrome_args = [], []
  for arg in positional_args:
    if arg.startswith('-'):
      chrome_args.append(arg)
    else:
      urls.append(arg)
  return [urls, chrome_args]


# Returns an object containing the arguments parsed from this script's command
# line.
def ParseArgs():
  # Customize usage and help to include options to be passed to chrome.exe.
  usage_text = '''%(prog)s [-h] [--interval INTERVAL] [--start_prompt]
                   [--exit_prompt] [--idlewakeups_dir IDLEWAKEUPS_DIR]
                   chrome_dir num_navigations url [url ...]
                   [-- --chrome_option ...]'''
  additional_help_text = '''optional arguments to chrome.exe, example:
  -- --enable-features=MyFeature --browser-startup-dialog
                        Must be at end of command, following the options
                        terminator "--"'''
  parser = argparse.ArgumentParser(
      epilog=additional_help_text,
      usage=usage_text,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      'chrome_dir', help='Directory containing chrome.exe and chromedriver.exe')
  parser.add_argument('num_navigations',
                      type=int,
                      help='Number of times to navigate through list of URLs')
  parser.add_argument('--interval',
                      '-i',
                      type=int,
                      help='Seconds to wait between navigations; default is 1')
  parser.add_argument('--start_prompt',
                      '-s',
                      action='store_true',
                      help='Wait for confirmation before starting navigation')
  parser.add_argument('--exit_prompt',
                      '-e',
                      action='store_true',
                      help='Wait for confirmation before exiting chrome.exe')
  parser.add_argument(
      '--idlewakeups_dir',
      help='Windows only; directory containing idlewakeups.exe, if using')
  parser.add_argument(
      'url',
      nargs='+',
      help='URL(s) to navigate, separated by spaces; must include scheme, '
      'e.g., "https://"')
  args = parser.parse_args()
  args.url, chrome_args = ParsePositionalArgs(args.url)
  if not args.url:
    parser.print_usage()
    print(os.path.basename(__file__) + ': error: missing URL argument')
    exit(EXIT_CODE_ERROR)
  for url in args.url:
    if not urllib.parse.urlparse(url).scheme:
      print(os.path.basename(__file__) +
            ': error: URL is missing required scheme (e.g., "https://"): ' + url)
      exit(EXIT_CODE_ERROR)
  return [args, chrome_args]


# If |path| does not exist, prints a generic error plus optional |error_message|
# and exits.
def ExitIfNotFound(path, error_message=None):
  if not os.path.exists(path):
    print('File not found: {}.'.format(path))
    if error_message:
      print(error_message)
    exit(EXIT_CODE_ERROR)


def main():
  # Parse arguments and check that file paths received are valid.
  args, chrome_args = ParseArgs()
  ExitIfNotFound(os.path.join(args.chrome_dir, 'chrome.exe'),
                 'Build target "chrome" to generate it first.')
  chromedriver_exe = os.path.join(args.chrome_dir, 'chromedriver.exe')
  ExitIfNotFound(chromedriver_exe,
                 'Build target "chromedriver" to generate it first.')
  if args.idlewakeups_dir:
    idlewakeups_exe = os.path.join(args.idlewakeups_dir, 'idlewakeups.exe')
    ExitIfNotFound(idlewakeups_exe)

  # Start chrome.exe. Disable chrome.exe's extensive logging to make reading
  # this script's output easier.
  chrome_options = webdriver.ChromeOptions()
  chrome_options.add_experimental_option('excludeSwitches', ['enable-logging'])
  for arg in chrome_args:
    chrome_options.add_argument(arg)
  driver = webdriver.Chrome(os.path.abspath(chromedriver_exe),
                            options=chrome_options)

  if args.start_prompt:
    driver.get(args.url[0])
    input('Press Enter to begin navigation...')

  # Start IdleWakeups, if using, passing the browser process's ID as its target.
  # IdleWakeups will monitor the browser process and its children. Other running
  # chrome.exe processes (i.e., those not launched by this script) are excluded.
  if args.idlewakeups_dir:
    launched_processes = psutil.Process(
        driver.service.process.pid).children(recursive=False)
    if not launched_processes:
      print('Error getting browser process ID for IdleWakeups.')
      exit()
    # Assume the first child process created by |driver| is the browser process.
    idlewakeups = subprocess.Popen([
        idlewakeups_exe,
        str(launched_processes[0].pid), '--stop-on-exit', '--tabbed'
    ],
                                   stdout=subprocess.PIPE)

  # Navigate through |args.url| list |args.num_navigations| times, then close
  # chrome.exe.
  interval = args.interval if args.interval else DEFAULT_INTERVAL
  for _ in range(args.num_navigations):
    for url in args.url:
      driver.get(url)
      time.sleep(interval)

  if args.exit_prompt:
    input('Press Enter to exit...')
  driver.quit()

  # Print IdleWakeups' output, if using.
  if args.idlewakeups_dir:
    print(idlewakeups.communicate()[0])


if __name__ == '__main__':
  sys.exit(main())
