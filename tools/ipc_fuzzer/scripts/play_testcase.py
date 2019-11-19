# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper around chrome.

Replaces all the child processes (renderer, GPU, plugins and utility) with the
IPC fuzzer. The fuzzer will then play back a specified testcase.

Depends on ipc_fuzzer being available on the same directory as chrome.
"""

from __future__ import print_function

import argparse
import os
import platform
import subprocess
import sys

CHROME_BINARY_FOR_PLATFORM_DICT = {
    'LINUX': 'chrome',
    'MAC': 'Chromium.app/Contents/MacOS/Chromium',
    'WINDOWS': 'chrome.exe',
}


def GetPlatform():
  platform = None
  if sys.platform.startswith('win'):
    platform = 'WINDOWS'
  elif sys.platform.startswith('linux'):
    platform = 'LINUX'
  elif sys.platform == 'darwin':
    platform = 'MAC'

  assert platform is not None
  return platform


def main():
  desc = 'Wrapper to run chrome with child processes replaced by IPC fuzzers'
  parser = argparse.ArgumentParser(description=desc)
  parser.add_argument(
      '--out-dir',
      dest='out_dir',
      default='out',
      help='output directory under src/ directory')
  parser.add_argument(
      '--build-type',
      dest='build_type',
      default='Release',
      help='Debug vs. Release build')
  parser.add_argument(
      '--gdb-browser',
      dest='gdb_browser',
      default=False,
      action='store_true',
      help='run browser process inside gdb')
  parser.add_argument('testcase', help='IPC file to be replayed')
  parser.add_argument(
      'chrome_args',
      nargs=argparse.REMAINDER,
      help='any additional arguments are passed to chrome')
  args = parser.parse_args()

  platform = GetPlatform()
  chrome_binary = CHROME_BINARY_FOR_PLATFORM_DICT[platform]
  fuzzer_binary = 'ipc_fuzzer_replay'
  if platform == 'WINDOWS':
    fuzzer_binary += '.exe'

  script_path = os.path.realpath(__file__)
  ipc_fuzzer_dir = os.path.join(os.path.dirname(script_path), os.pardir)
  src_dir = os.path.abspath(os.path.join(ipc_fuzzer_dir, os.pardir, os.pardir))
  out_dir = os.path.join(src_dir, args.out_dir)
  build_dir = os.path.join(out_dir, args.build_type)

  chrome_path = os.path.join(build_dir, chrome_binary)
  if not os.path.exists(chrome_path):
    print('chrome executable not found at ', chrome_path)
    return 1

  fuzzer_path = os.path.join(build_dir, fuzzer_binary)
  if not os.path.exists(fuzzer_path):
    print('fuzzer executable not found at ', fuzzer_path)
    print('ensure GYP_DEFINES="enable_ipc_fuzzer=1" and build target ' +
          fuzzer_binary + '.')
    return 1

  prefixes = {
      '--renderer-cmd-prefix',
      '--plugin-launcher',
      '--ppapi-plugin-launcher',
      '--utility-cmd-prefix',
  }

  chrome_command = [
      chrome_path,
      '--ipc-fuzzer-testcase=' + args.testcase,
      '--no-sandbox',
      '--disable-kill-after-bad-ipc',
      '--disable-mojo-channel',
  ]

  if args.gdb_browser:
    chrome_command = ['gdb', '--args'] + chrome_command

  launchers = {}
  for prefix in prefixes:
    launchers[prefix] = fuzzer_path

  for arg in args.chrome_args:
    if arg.find('=') != -1:
      switch, value = arg.split('=', 1)
      if switch in prefixes:
        launchers[switch] = value + ' ' + launchers[switch]
        continue
    chrome_command.append(arg)

  for switch, value in launchers.items():
    chrome_command.append(switch + '=' + value)

  command_line = ' '.join(['\'' + arg + '\'' for arg in chrome_command])
  print('Executing: ' + command_line)

  return subprocess.call(chrome_command)


if __name__ == '__main__':
  sys.exit(main())
