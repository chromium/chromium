# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Displays a trace file in a browser.
"""

import logging
import os
import subprocess
import sys
import webbrowser

import flag_utils


def DisplayInBrowser(trace_file, trace_format='proto'):
  """Displays trace in browser.

  Args:
    trace_file: Saved trace filename.
    trace_format: Storage format of trace file.
  """
  if trace_format == 'json':
    raise Exception('The --view option and --trace_format=json are not'
                    'supported together')
  if trace_format == 'proto':
    flag_utils.GetTracingLogger().info('Opening trace in browser')
    open_trace_ui_path = os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir,
        'third_party/perfetto/tools/open_trace_in_ui')
    trace_file_path = os.path.abspath(trace_file)
    cmd = [open_trace_ui_path, '-i', trace_file_path]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stderr = p.communicate()[1]
    if p.returncode != 0:
      raise RuntimeError('failed: ' + stderr)
  elif sys.platform == 'darwin':
    os.system('/usr/bin/open %s' % os.path.abspath(trace_file))
  else:
    webbrowser.open(trace_file)
