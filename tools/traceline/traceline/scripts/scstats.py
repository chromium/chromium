#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys

from syscalls import syscalls


def parseEvents(z):
  calls = { }
  for e in z:
    if e['eventtype'] == 'EVENT_TYPE_SYSCALL' and e['done'] > 0:
      delta = e['done'] - e['ms']
      syscall = e['syscall']
      tid = e['thread']
      ms = e['ms']
      calls[syscall] = calls.get(syscall, 0) + delta
      print('%f - %f - %x - %d %s' % (delta, ms, tid, syscall,
                                      syscalls.get(syscall, 'unknown')))

  #for syscall, delta in calls.items():
  #  print('%f - %d %s' % (delta, syscall, syscalls.get(syscall, 'unknown')))


def main():
  execfile(sys.argv[1])


if __name__ == '__main__':
  main()
