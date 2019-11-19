#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome Version Tool

Scrapes Chrome channel information and prints out the requested nugget of
information.
"""

from __future__ import print_function

import json
import optparse
import os
import string
import sys
import urllib

URL = 'https://omahaproxy.appspot.com/json'


def main():
  try:
    data = json.load(urllib.urlopen(URL))
  except Exception as e:
    print('Error: could not load %s\n\n%s' % (URL, str(e)))
    return 1

  # Iterate to find out valid values for OS, channel, and field options.
  oses = set()
  channels = set()
  fields = set()

  for os_versions in data:
    oses.add(os_versions['os'])

    for version in os_versions['versions']:
      for field in version:
        if field == 'channel':
          channels.add(version['channel'])
        else:
          fields.add(field)

  oses = sorted(oses)
  channels = sorted(channels)
  fields = sorted(fields)

  # Command line parsing fun begins!
  usage = ('%prog [options]\n'
           'Print out information about a particular Chrome channel.')
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('-o', '--os',
                    choices=oses,
                    default='win',
                    help='The operating system of interest: %s '
                         '[default: %%default]' % ', '.join(oses))
  parser.add_option('-c', '--channel',
                    choices=channels,
                    default='stable',
                    help='The channel of interest: %s '
                         '[default: %%default]' % ', '.join(channels))
  parser.add_option('-f', '--field',
                    choices=fields,
                    default='version',
                    help='The field of interest: %s '
                         '[default: %%default] ' % ', '.join(fields))
  (opts, args) = parser.parse_args()

  # Print out requested data if available.
  for os_versions in data:
    if os_versions['os'] != opts.os:
      continue

    for version in os_versions['versions']:
      if version['channel'] != opts.channel:
        continue

      if opts.field not in version:
        continue

      print(version[opts.field])
      return 0

  print('Error: unable to find %s for Chrome %s %s.' % (opts.field, opts.os,
                                                        opts.channel))
  return 1

if __name__ == '__main__':
  sys.exit(main())
