#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes an input JSON, and filters out all system call events that
took less than 0.2ms.

This helps trim down the JSON data to only the most interesting / time critical
events.
"""

from __future__ import print_function

import sys
import re


def parseEvents(z):
  print('parseEvents([')
  for e in z:
    if 'ms' in e and 'done' in e:
      dur = e['done'] - e['ms']
      if dur < 0.2:
        continue
    # Ugly regex to remove the L suffix on large python numbers.
    print('%s,' % re.sub('([0-9])L\\b', '\\1', str(e)))
  print('])')


def main():
  execfile(sys.argv[1])


if __name__ == '__main__':
  main()
