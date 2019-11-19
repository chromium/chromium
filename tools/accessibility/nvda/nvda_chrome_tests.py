#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Semi-automated tests of Chrome with NVDA.

This file performs (semi) automated tests of Chrome with NVDA
(NonVisual Desktop Access), a popular open-source screen reader for
visually impaired users on Windows. It works by launching Chrome in a
subprocess, then launching NVDA in a special environment that simulates
speech rather than actually speaking, and ignores all events coming from
processes other than a specific Chrome process ID. Each test automates
Chrome with a series of actions and asserts that NVDA gives the expected
feedback in response.

The tests are "semi" automated in the sense that they are not intended to be
run from any developer machine, or on a buildbot - it requires setting up the
environment according to the instructions in README.txt, then running the
test script, then filing bugs for any potential failures. If the environment
is set up correctly, the actual tests should run automatically and unattended.
"""

from __future__ import print_function

import os
import pywinauto
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

CHROME_PROFILES_PATH = os.path.join(os.getcwd(), 'chrome_profiles')
CHROME_PATH = os.path.join(os.environ['USERPROFILE'],
                           'AppData',
                           'Local',
                           'Google',
                           'Chrome SxS',
                           'Application',
                           'chrome.exe')
NVDA_PATH = os.path.join(os.getcwd(),
                         'nvdaPortable',
                         'nvda_noUIAccess.exe')
NVDA_PROCTEST_PATH = os.path.join(os.getcwd(),
                                  'nvda-proctest')
NVDA_LOGPATH = os.path.join(os.getcwd(),
                            'nvda_log.txt')
WAIT_FOR_SPEECH_TIMEOUT_SECS = 3.0

class NvdaChromeTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    print('user data: %s' % CHROME_PROFILES_PATH)
    print('chrome: %s' % CHROME_PATH)
    print('nvda: %s' % NVDA_PATH)
    print('nvda_proctest: %s' % NVDA_PROCTEST_PATH)

    tasklist = subprocess.Popen("tasklist", shell=True, stdout=subprocess.PIPE)
    tasklist_output = tasklist.communicate()[0].decode('utf8').split('\r\n')
    for task in tasklist_output:
      if (task.split(' ', 1)[0] == "nvda.exe"):
        print("nvda.exe is running!  Please kill it before running these tests")
        sys.exit()

    print()
    print('Clearing user data directory and log file from previous runs')
    if os.access(NVDA_LOGPATH, os.F_OK):
      os.remove(NVDA_LOGPATH)
    if os.access(CHROME_PROFILES_PATH, os.F_OK):
      shutil.rmtree(CHROME_PROFILES_PATH)
    os.mkdir(CHROME_PROFILES_PATH, 0o777)

    def handler(signum, frame):
      print('Test interrupted, attempting to kill subprocesses.')
      self.tearDown()
      sys.exit()
    signal.signal(signal.SIGINT, handler)

  def setUp(self):
    user_data_dir = tempfile.mkdtemp(dir = CHROME_PROFILES_PATH)
    args = [CHROME_PATH,
            '--user-data-dir=%s' % user_data_dir,
            '--no-first-run',
            'about:blank']
    print()
    print(' '.join(args))
    self._chrome_proc = subprocess.Popen(args)
    self._chrome_proc.poll()
    if self._chrome_proc.returncode is None:
      print('Chrome is running')
    else:
      print('Chrome exited with code', self._chrome_proc.returncode)
      sys.exit()
    print('Chrome pid: %d' % self._chrome_proc.pid)

    os.environ['NVDA_SPECIFIC_PROCESS'] = str(self._chrome_proc.pid)

    args = [NVDA_PATH,
            '-m',
            '-c',
            NVDA_PROCTEST_PATH,
            '-f',
            NVDA_LOGPATH]
    self._nvda_proc = subprocess.Popen(args)
    self._nvda_proc.poll()
    if self._nvda_proc.returncode is None:
      print('NVDA is running')
    else:
      print('NVDA exited with code', self._nvda_proc.returncode)
      sys.exit()
    print('NVDA pid: %d' % self._nvda_proc.pid)

    app = pywinauto.application.Application()
    app.connect(process = self._chrome_proc.pid)
    self._pywinauto_window = app.top_window()
    self.last_nvda_log_line = 0;

  def tearDown(self):
    print()
    print('Shutting down')

    self._chrome_proc.poll()
    if self._chrome_proc.returncode is None:
      print('Killing Chrome subprocess')
      self._chrome_proc.kill()
    else:
      print('Chrome already died.')

    self._nvda_proc.poll()
    if self._nvda_proc.returncode is None:
      print('Killing NVDA subprocess')
      self._nvda_proc.kill()
    else:
      print('NVDA already died.')

  def _GetSpeechFromNvdaLogFile(self):
    """Return everything NVDA would have spoken as a list of strings.

    Parses lines like this from NVDA's log file:
      Speaking [LangChangeCommand ('en'), u'Google Chrome', u'window']
      Speaking character u'slash'

    Returns a single list of strings like this:
      [u'Google Chrome', u'window', u'slash']
    """
    if not os.access(NVDA_LOGPATH, os.F_OK):
      return []
    lines = open(NVDA_LOGPATH).readlines()[self.last_nvda_log_line:]
    regex = re.compile(r"u'((?:[^\'\\]|\\.)*)\'")
    result = []
    for line in lines:
      for m in regex.finditer(line):
        speech_with_whitespace = m.group(1)
        speech_stripped = re.sub(r'\s+', ' ', speech_with_whitespace).strip()
        result.append(speech_stripped)
    self.last_nvda_log_line = len(lines) - 1
    return result

  def _ArrayInArray(self, lines, expected):
    positions = len(lines) - len(expected) + 1;
    if (positions >= 0):
      # loop through the number of positions that the subset can hold
      for index in range(positions):
        if (lines[index : index + len(expected)] == expected):
          return True
    return False

  def _TestForSpeech(self, expected):
    """Block until the last speech in NVDA's log file is the given string(s).

    Repeatedly parses the log file until the last speech line(s) in the
    log file match the given strings, or it times out.

    Args:
      expected: string or a list of string - only succeeds if these are the last
        strings spoken, in order.
    """
    if type(expected) is type(''):
      expected = [expected]
    start_time = time.time()
    while True:
      lines = self._GetSpeechFromNvdaLogFile()

      if self._ArrayInArray(lines, expected):
        return True

      if time.time() - start_time >= WAIT_FOR_SPEECH_TIMEOUT_SECS:
        self.fail("Test for expected speech failed.\n\nExpected:\n" +
          str(expected) +
          ".\n\nActual:\n" + str(lines));
        return False
      time.sleep(0.1)

  #
  # Tests
  #

  def testTypingInOmnibox(self):
    # Ctrl+A: Select all.
    self._pywinauto_window.TypeKeys('^l')
    self._TestForSpeech(["main tool bar"]);

    self._pywinauto_window.TypeKeys('xyz')
    self._pywinauto_window.TypeKeys('^a')
    self._TestForSpeech(["selected about:blank"]);

  def testFocusToolbarButton(self):
    # Alt+Shift+T.
    self._pywinauto_window.TypeKeys('%+T')
    self._TestForSpeech('Reload button Reload this page')
    # this is flakey because sometimes this will be a stop button too.

  def testReadAllOnPageLoad(self):
    # Load data url.

    # Focus the url bar with control-L
    self._pywinauto_window.TypeKeys('^l')

    self._pywinauto_window.TypeKeys('^a')

    self._pywinauto_window.TypeKeys('data:text/html,Hello<p>World.')
    self._pywinauto_window.TypeKeys('{ENTER}')

    self._TestForSpeech(
        ['Hello',
         'World.'])

if __name__ == '__main__':
  unittest.main()
