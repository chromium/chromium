# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging
import subprocess
import platform
import time


def _GetLockFilePath(server_num):
  return "/tmp/.X11-unix/X{}".format(server_num)


def ShouldStartXvfb():
  # TODO(crbug.com/40631966): Note that you can locally change this to return
  # False to diagnose timeouts for dev server tests.
  return platform.system() == 'Linux'


def StartXvfb():
  # Get a number of an available server.
  server_num = 99
  while os.path.exists(_GetLockFilePath(server_num)):
    server_num += 1

  display = ':{}'.format(server_num)
  xvfb_command = ['Xvfb', display, '-screen', '0', '1024x769x24', '-ac']
  xvfb_process = subprocess.Popen(xvfb_command,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT)
  time.sleep(0.2)
  returncode = xvfb_process.poll()
  if returncode is None:
    os.environ['DISPLAY'] = display

    # Use print instead of logging to ensure that the recommendation to remove
    # the lock file is displayed regardless of verbosity settings.
    print('Started Xvfb on display %s.' % display)
    print('If test fails to exit cleanly, it is recommended that you manually'
          ' remove the file "%s".\n' % _GetLockFilePath(server_num))
  else:
    logging.error('Xvfb did not start, returncode: %s, stdout:\n%s', returncode,
                  xvfb_process.stdout.read())
    xvfb_process = None
  return xvfb_process
