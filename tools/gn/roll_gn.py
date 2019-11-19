#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to roll the latest GN into Chromium.

Usage:

  $ roll_gn.py [revision]

The script will modify //DEPS and //buildtools/DEPS to point to most recent
revision of GN and create a commit with a list of the changes included in the
roll.

If you don't want to roll to the latest version, you can specify the SHA-1
to roll to as an argument to the script, and it will be used instead.

The script will not actually upload anything for review, so you will still
need to do that afterwards.
"""

from __future__ import print_function

import argparse
import json
import os
import subprocess
import sys
import urllib2


THIS_DIR = os.path.dirname(__file__)
SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))


def main():
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('revision', nargs='?',
                      help='The SHA-1 to roll to; will use the latest '
                           'if not specified')
  args = parser.parse_args()

  deps_path = os.path.join(SRC_DIR, 'DEPS')
  try:
    gn_version = subprocess.check_output([
      'gclient',
      'getdep',
      '--deps-file=%s' % deps_path,
      '--var=gn_version']).strip()
    current_revision = gn_version[len('git_revision:'):]
  except Exception as e:
    print('Could not determine current GN version from DEPS: %s' % str(e),
          file=sys.stderr)
    return 1

  if args.revision is None:
    try:
      args.revision = subprocess.check_output([
          'git',
          'ls-remote',
          'https://gn.googlesource.com/gn.git',
          'master']).split()[0]
    except Exception as e:
      print('Failed to fetch current revision: %s' % str(e), file=sys.stderr)
      return 1

  try:
    url = ('https://gn.googlesource.com/gn.git/+log/%s..%s?format=JSON' %
           (current_revision, args.revision))
    resp = urllib2.urlopen(url)
  except Exception as e:
    print('Failed to fetch log via %s: %s' % (url, str(e)), file=sys.stderr)
    return 1

  # skip over the first 5 chars, to find the start of the JSON response.
  log = json.loads(resp.read()[5:])

  changes = [(change['commit'], change['message'].splitlines()[0])
             for change in log['log']]

  subprocess.call([
    'gclient',
    'setdep',
    '--deps-file=%s' % deps_path,
    '--var=gn_version=git_revision:%s' % args.revision])
  subprocess.call([
    'gclient',
    'setdep',
    '--deps-file=%s' % os.path.join(SRC_DIR, 'buildtools', 'DEPS'),
    '--var=gn_version=git_revision:%s' % args.revision])

  change_lines = '\n'.join('    %s %s' % (tup[0][:8], tup[1])
                           for tup in changes)
  commit_message = '''\
Roll GN from %s..%s

%s''' % (current_revision[:8], args.revision[:8], change_lines)

  subprocess.call(['git', 'commit', '-a', '-m', commit_message])

  return 0


if __name__ == '__main__':
  sys.exit(main())
