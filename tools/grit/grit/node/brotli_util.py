# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Framework for compressing resources using Brotli."""

import subprocess

__brotli_executable = None


def SetBrotliCommand(brotli):
  # brotli is a list. In production it contains the path to the Brotli executable.
  # During testing it contains [python, mock_brotli.py] for testing on Windows.
  global __brotli_executable
  __brotli_executable = brotli


def BrotliCompress(data):
  if not __brotli_executable:
    raise Exception('Add "use_brotli = true" to you GN grit(...) target ' +
                    'if you want to use brotli.')
  compress = subprocess.Popen(__brotli_executable + ['-', '-f'],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  return compress.communicate(data)[0]

def IsInitialized():
  global __brotli_executable
  return __brotli_executable is not None
