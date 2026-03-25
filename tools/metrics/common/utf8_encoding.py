# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Provides helpers to set the output encoding of the printing metrics tools"""

import sys


def setup_stdout_and_stderr_utf8_encoding():
  """Sets standard output and standard error encoding to utf8"""
  # We need to check for reconfigure being present because it might not allow
  # that. Specifically this is typically the case for mocked stdout/stderr.
  if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

  if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8')
