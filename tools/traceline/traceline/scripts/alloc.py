#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys

from syscalls import syscalls


def parseEvents(z):
  calls = { }
  for e in z:
    if e['eventtype'] == 'EVENT_TYPE_SYSCALL' and e['syscall'] == 17:
      delta = e['done'] - e['ms']
      tid = e['thread']
      ms = e['ms']
      print('%f - %f - %x' % (delta, ms, tid))


def main():
  execfile(sys.argv[1])


if __name__ == '__main__':
  sys.exit(main())
