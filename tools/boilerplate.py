#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create files with copyright boilerplate and header include guards.

Usage: tools/boilerplate.py path/to/file.{h,cc}
"""

from __future__ import print_function

from datetime import date
import os
import os.path
import sys

LINES = [
    'Copyright %d The Chromium Authors. All rights reserved.' %
        date.today().year,
    'Use of this source code is governed by a BSD-style license that can be',
    'found in the LICENSE file.'
]

EXTENSIONS_TO_COMMENTS = {
    'h': '//',
    'cc': '//',
    'mm': '//',
    'js': '//',
    'py': '#',
    'gn': '#',
    'gni': '#',
    'mojom': '//',
    'typemap': '#',
}

def _GetHeader(filename):
  _, ext = os.path.splitext(filename)
  ext = ext[1:]
  comment = EXTENSIONS_TO_COMMENTS[ext] + ' '
  return '\n'.join([comment + line for line in LINES])


def _CppHeader(filename):
  guard = filename.upper() + '_'
  for char in '/\\.+':
    guard = guard.replace(char, '_')
  return '\n'.join([
    '',
    '#ifndef ' + guard,
    '#define ' + guard,
    '',
    '#endif  // ' + guard,
    ''
  ])


def _RemoveTestSuffix(filename):
  base, _ = os.path.splitext(filename)
  suffixes = [ '_test', '_unittest', '_browsertest' ]
  for suffix in suffixes:
    l = len(suffix)
    if base[-l:] == suffix:
      return base[:-l]
  return base


def _IsIOSFile(filename):
  if os.path.splitext(os.path.basename(filename))[0].endswith('_ios'):
    return True
  if 'ios' in filename.split(os.path.sep):
    return True
  return False


def _FilePathSlashesToCpp(filename):
  return filename.replace('\\', '/')


def _CppImplementation(filename):
  return '\n#include "' + _FilePathSlashesToCpp(_RemoveTestSuffix(filename)) \
    + '.h"\n'


def _ObjCppImplementation(filename):
  implementation = '\n#import "' + _RemoveTestSuffix(filename) + '.h"\n'
  if not _IsIOSFile(filename):
    return implementation
  implementation += '\n'
  implementation += '#if !defined(__has_feature) || !__has_feature(objc_arc)\n'
  implementation += '#error "This file requires ARC support."\n'
  implementation += '#endif\n'
  return implementation


def _CreateFile(filename):
  contents = _GetHeader(filename) + '\n'

  if filename.endswith('.h'):
    contents += _CppHeader(filename)
  elif filename.endswith('.cc'):
    contents += _CppImplementation(filename)
  elif filename.endswith('.mm'):
    contents += _ObjCppImplementation(filename)

  fd = open(filename, 'wb')
  fd.write(contents)
  fd.close()


def Main():
  files = sys.argv[1:]
  if len(files) < 1:
    print(
        'Usage: boilerplate.py path/to/file.h path/to/file.cc', file=sys.stderr)
    return 1

  # Perform checks first so that the entire operation is atomic.
  for f in files:
    _, ext = os.path.splitext(f)
    if not ext[1:] in EXTENSIONS_TO_COMMENTS:
      print('Unknown file type for %s' % f, file=sys.stderr)
      return 2

    if os.path.exists(f):
      print('A file at path %s already exists' % f, file=sys.stderr)
      return 2

  for f in files:
    _CreateFile(f)


if __name__ == '__main__':
  sys.exit(Main())
