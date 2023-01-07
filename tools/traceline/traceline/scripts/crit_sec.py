#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys
import os

from syscalls import syscalls


def parseEvents(z):
  crits =  { }
  calls = { }
  for e in z:
    if (e['eventtype'] == 'EVENT_TYPE_ENTER_CS' or
        e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS' or
        e['eventtype'] == 'EVENT_TYPE_LEAVE_CS'):
      cs = e['critical_section']
      if cs not in crits:
        crits[cs] = [ ]
      crits[cs].append(e)

#  for cs, es in crits.iteritems():
#    print('cs: 0x%08x' % cs)
#    for e in es:
#      print('  0x%08x - %s - %f' % (e['thread'], e['eventtype'], e['ms']))

  for cs, es in crits.iteritems():
    print('cs: 0x%08x' % cs)

    tid_stack = [ ]
    for e in es:
      if e['eventtype'] == 'EVENT_TYPE_ENTER_CS':
        tid_stack.append(e)
      elif e['eventtype'] == 'EVENT_TYPE_TRYENTER_CS':
        if e['retval'] != 0:
          tid_stack.append(e)
      elif e['eventtype'] == 'EVENT_TYPE_LEAVE_CS':
        if not tid_stack:
          raise repr(e)
        tid = tid_stack.pop()
        if tid['thread'] != e['thread']:
          raise repr(tid) + '--' + repr(e)

    # Critical section left locked?
    if tid_stack:
      #raise repr(tid_stack)
      pass


def main():
  execfile(sys.argv[1])


if __name__ == '__main__':
  main()
