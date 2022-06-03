# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Simple utilities for printf debugging."""

import sys


def _joinArgs(args):
  return ' '.join(str(t) for t in args) + '\n'


def out(*args):
  sys.stderr.write(_joinArgs(args))


def outR(*args):
  sys.stderr.write('\x1b[91m%s\x1b[0m' % _joinArgs(args))


def outG(*args):
  sys.stderr.write('\x1b[92m%s\x1b[0m' % _joinArgs(args))


def outY(*args):
  sys.stderr.write('\x1b[93m%s\x1b[0m' % _joinArgs(args))


def outB(*args):
  sys.stderr.write('\x1b[94m%s\x1b[0m' % _joinArgs(args))


def outM(*args):
  sys.stderr.write('\x1b[95m%s\x1b[0m' % _joinArgs(args))


def outC(*args):
  sys.stderr.write('\x1b[96m%s\x1b[0m' % _joinArgs(args))
