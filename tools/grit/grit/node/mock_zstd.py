#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mock zstd executable and test helpers."""

import contextlib
import os
import sys

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit import util
from grit.node import zstd_util


@contextlib.contextmanager
def SetUp():
  zstd_util.SetZstdCommand([sys.executable, os.path.abspath(__file__)])
  try:
    yield
  finally:
    zstd_util.SetZstdCommand(None)


def ExpectedOutputForFile(filename, level_args=b'-15'):
  size = len(
    util.ReadFile(
      os.path.join(util.PathFromRoot('grit/testdata'), filename), util.BINARY
    )
  )
  return b'-q -c --no-check --stream-size=%d %s size=%d' % (
    size,
    level_args,
    size,
  )


if __name__ == '__main__':
  data = sys.stdin.buffer.read()
  sys.stdout.write('%s size=%d' % (' '.join(sys.argv[1:]), len(data)))
