# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to bisect field trials to pin point a culprit for certain behavior.

Chrome runs with many experiments and variations (field trials) that are
randomly selected based on a configuration from a server. They lead to
different code paths and different Chrome behaviors. When a bug is caused by
one of the experiments or variations, it is useful to be able to bisect into
the set and pin-point which one is responsible.

Go to chrome://version/?show-variations-cmd. At the bottom, a few commandline
switches define the current experiments and variations Chrome runs with.

Sample use:

python bisect_variations.py --input-file="variations_cmd.txt"
    --output-dir=".\out" --browser=canary --url="https://www.youtube.com/"

"variations_cmd.txt" is the command line switches data saved from
chrome://version/?show-variations-cmd.

Run with --help to get a complete list of options this script runs with.
"""

from __future__ import print_function

import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

import split_variations_cmd

_CHROME_PATH_WIN = {
  # The following three paths are relative to %ProgramFiles(x86)%
  "stable": r"Google\Chrome\Application\chrome.exe",
  "beta": r"Google\Chrome\Application\chrome.exe",
  "dev": r"Google\Chrome Dev\Application\chrome.exe",
  # The following two paths are relative to %LOCALAPPDATA%
  "canary": r"Google\Chrome SxS\Application\chrome.exe",
  "chromium": r"Chromium\Application\chrome.exe",
}

_CHROME_PATH_MAC = {
  "stable": r"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
  "beta": r"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
  "dev": r"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
  "canary": (r"/Applications/Google Chrome Canary.app/Contents/MacOS/"
             r"Google Chrome Canary"),
}

_CHROME_PATH_LINUX = {
  "stable": r"/usr/bin/google-chrome",
  "beta": r"/usr/bin/google-chrome-beta",
  "dev": r"/usr/bin/google-chrome-unstable",
  "chromium": r"/usr/bin/chromium",
}

def _GetSupportedBrowserTypes():
  """Returns the supported browser types on this platform."""
  if sys.platform.startswith('win'):
    return _CHROME_PATH_WIN.keys()
  if sys.platform == 'darwin':
    return _CHROME_PATH_MAC.keys();
  if sys.platform.startswith('linux'):
    return _CHROME_PATH_LINUX.keys();
  raise NotImplementedError('Unsupported platform')


def _LocateBrowser_Win(browser_type):
  """Locates browser executable path based on input browser type.

  Args:
      browser_type: 'stable', 'beta', 'dev', 'canary', or 'chromium'.

  Returns:
      Browser executable path.
  """
  if browser_type in ['stable', 'beta', 'dev']:
    return os.path.join(os.getenv('ProgramFiles(x86)'),
                        _CHROME_PATH_WIN[browser_type])
  else:
    assert browser_type in ['canary', 'chromium']
    return os.path.join(os.getenv('LOCALAPPDATA'),
                        _CHROME_PATH_WIN[browser_type])


def _LocateBrowser_Mac(browser_type):
  """Locates browser executable path based on input browser type.

  Args:
      browser_type: A supported browser type on Mac.

  Returns:
      Browser executable path.
  """
  return _CHROME_PATH_MAC[browser_type]


def _LocateBrowser_Linux(browser_type):
  """Locates browser executable path based on input browser type.

  Args:
      browser_type: A supported browser type on Linux.

  Returns:
      Browser executable path.
  """
  return _CHROME_PATH_LINUX[browser_type]


def _LocateBrowser(browser_type):
  """Locates browser executable path based on input browser type.

  Args:
      browser_type: A supported browser types on this platform.

  Returns:
      Browser executable path.
  """
  supported_browser_types = _GetSupportedBrowserTypes()
  if browser_type not in supported_browser_types:
    raise ValueError('Invalid browser type. Supported values are: %s.' %
                         ', '.join(supported_browser_types))
  if sys.platform.startswith('win'):
    return _LocateBrowser_Win(browser_type)
  elif sys.platform == 'darwin':
    return _LocateBrowser_Mac(browser_type)
  elif sys.platform.startswith('linux'):
    return _LocateBrowser_Linux(browser_type)
  else:
    raise NotImplementedError('Unsupported platform')


def _LoadVariations(filename):
  """Reads variations commandline switches from a file.

  Args:
      filename: A file that contains variations commandline switches.

  Returns:
      A list of commandline switches.
  """
  with open(filename, 'r') as f:
    data = f.read().replace('\n', ' ')
  switches = split_variations_cmd.ParseCommandLineSwitchesString(data)
  return ['--%s=%s' % (switch_name, switch_value) for
          switch_name, switch_value in switches.items()]


def _BuildBrowserArgs(user_data_dir, extra_browser_args, variations_args):
  """Builds commandline switches browser runs with.

  Args:
      user_data_dir: A path that is used as user data dir.
      extra_browser_args: A list of extra commandline switches browser runs
          with.
      variations_args: A list of commandline switches that defines the
          variations cmd browser runs with.

  Returns:
      A list of commandline switches.
  """
  # Make sure each run is fresh, but avoid first run setup steps.
  browser_args = [
    '--no-first-run',
    '--no-default-browser-check',
    '--user-data-dir=%s' % user_data_dir,
  ]
  browser_args.extend(extra_browser_args)
  browser_args.extend(variations_args)
  return browser_args


def _RunVariations(browser_path, url, extra_browser_args, variations_args):
  """Launches browser with given variations.

  Args:
      browser_path: Browser executable file.
      url: The webpage URL browser goes to after it launches.
      extra_browser_args: A list of extra commandline switches browser runs
          with.
      variations_args: A list of commandline switches that defines the
          variations cmd browser runs with.

  Returns:
      A set of (returncode, stdout, stderr) from browser subprocess.
  """
  command = [os.path.abspath(browser_path)]
  if url:
    command.append(url)
  tempdir = tempfile.mkdtemp(prefix='bisect_variations_tmp')
  command.extend(_BuildBrowserArgs(user_data_dir=tempdir,
                                   extra_browser_args=extra_browser_args,
                                   variations_args=variations_args))
  logging.debug(' '.join(command))

  subproc = subprocess.Popen(
      command, bufsize=-1, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = subproc.communicate()
  shutil.rmtree(tempdir, True)
  return (subproc.returncode, stdout, stderr)


def _AskCanReproduce(exit_status, stdout, stderr):
  """Asks whether running Chrome with given variations reproduces the issue.

  Args:
      exit_status: Chrome subprocess return code.
      stdout: Chrome subprocess stdout.
      stderr: Chrome subprocess stderr.

  Returns:
      One of ['y', 'n', 'r']:
        'y': yes
        'n': no
        'r': retry
  """
  # Loop until we get a response that we can parse.
  while True:
    response = raw_input('Can we reproduce with given variations file '
                         '[(y)es/(n)o/(r)etry/(s)tdout/(q)uit]: ').lower()
    if response in ('y', 'n', 'r'):
      return response
    if response == 'q':
      sys.exit()
    if response == 's':
      logging.info(stdout)
      logging.info(stderr)


def Bisect(browser_type, url, extra_browser_args, variations_file, output_dir):
  """Bisect variations interactively.

  Args:
      browser_type: One of the supported browser type on this platform. See
          --help for the list.
      url: The webpage URL browser launches with.
      extra_browser_args: A list of commandline switches browser runs with.
      variations_file: A file contains variations commandline switches that
          need to be bisected.
      output_dir: A folder where intermediate bisecting data are stored.
  """
  browser_path = _LocateBrowser(browser_type)
  runs = [variations_file]
  while runs:
    run = runs[0]
    print('Run Chrome with variations file', run)
    variations_args = _LoadVariations(run)
    exit_status, stdout, stderr = _RunVariations(
        browser_path=browser_path, url=url,
        extra_browser_args=extra_browser_args,
        variations_args=variations_args)

    answer = _AskCanReproduce(exit_status, stdout, stderr)
    if answer == 'y':
      runs = split_variations_cmd.SplitVariationsCmdFromFile(run, output_dir)
      if len(runs) == 1:
        # Can divide no further.
        print('Bisecting succeeded:', ' '.join(variations_args))
        return
    elif answer == 'n':
      if len(runs) == 1:
        raise Exception('Bisecting failed: should reproduce but did not: %s' %
                        ' '.join(variations_args))
      runs = runs[1:]
    else:
      assert answer == 'r'


def main():
  parser = optparse.OptionParser()
  parser.add_option("--browser",
                    help="select which browser to run. Options include: %s."
                    " By default, stable is selected." %
                        ", ".join(_GetSupportedBrowserTypes()))
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="print out debug information.")
  parser.add_option("--extra-browser-args",
                    help="specify extra command line switches for the browser "
                    "that are separated by spaces (quoted).")
  parser.add_option("--url",
                    help="specify the webpage URL the browser launches with. "
                    "This is optional.")
  parser.add_option("--input-file",
                    help="specify the filename that contains variations cmd "
                    "to bisect. This has to be specified.")
  parser.add_option("--output-dir",
                    help="specify a folder where output files are saved. "
                    "If not specified, it is the folder of the input file.")
  options, _ = parser.parse_args()
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)
  if options.input_file is None:
    raise ValueError('Missing input through --input-file.')
  output_dir = options.output_dir
  if output_dir is None:
    output_dir, _ = os.path.split(options.input_file)
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)
  browser_type = options.browser
  if browser_type is None:
    browser_type = 'stable'
  extra_browser_args = []
  if options.extra_browser_args is not None:
    extra_browser_args = options.extra_browser_args.split()
  Bisect(browser_type=browser_type, url=options.url,
         extra_browser_args=extra_browser_args,
         variations_file=options.input_file, output_dir=output_dir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
