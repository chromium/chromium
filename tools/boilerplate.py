#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create files with copyright boilerplate and header include guards.

Usage: tools/boilerplate.py path/to/file.{h,cc}
"""

from __future__ import print_function, unicode_literals

from datetime import date
import io
import os
import os.path
import sys

LINES = [
    f'Copyright {date.today().year} The Chromium Authors',
    'Use of this source code is governed by a BSD-style license that can be',
    'found in the LICENSE file.'
]

NO_COMPILE_LINES = [
    'This is a "No Compile Test" suite.',
    'https://dev.chromium.org/developers/testing/no-compile-tests'
]

EXTENSIONS_TO_COMMENTS = {
    'cc': '//',
    'gn': '#',
    'gni': '#',
    'h': '//',
    'js': '//',
    'mm': '//',
    'mojom': '//',
    'nc': '//',
    'proto': '//',
    'py': '#',
    'swift': '//',
    'ts': '//',
    'typemap': '#',
}


def _GetHeaderImpl(filename, lines):
  _, ext = os.path.splitext(filename)
  ext = ext[1:]
  comment = EXTENSIONS_TO_COMMENTS[ext] + ' '
  return '\n'.join([comment + line for line in lines])


def _GetHeader(filename):
  return _GetHeaderImpl(filename, LINES)


def _GetNoCompileHeader(filename):
  assert (filename.endswith(".nc"))
  return '\n' + _GetHeaderImpl(filename, NO_COMPILE_LINES)


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


def _RemoveCurrentDirectoryPrefix(filename):
  current_dir_prefixes = [os.curdir + os.sep]
  if os.altsep is not None:
    current_dir_prefixes.append(os.curdir + os.altsep)
  for prefix in current_dir_prefixes:
    if filename.startswith(prefix):
      return filename[len(prefix):]
  return filename


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
  return '\n#import "' + _FilePathSlashesToCpp(_RemoveTestSuffix(filename)) \
    + '.h"\n'


def _CreateFile(filename):
  filename = _RemoveCurrentDirectoryPrefix(filename)

  contents = _GetHeader(filename) + '\n'

  if filename.endswith('.h'):
    contents += _CppHeader(filename)
  elif filename.endswith('.cc'):
    contents += _CppImplementation(filename)
  elif filename.endswith('.nc'):
    contents += _GetNoCompileHeader(filename) + '\n'
    contents += _CppImplementation(filename)
  elif filename.endswith('.mm'):
    contents += _ObjCppImplementation(filename)

  with io.open(filename, mode='w', newline='\n') as fd:
    fd.write(contents)


# A file is safe to overwrite if it's an empty file we can write to.
def _IsSafeToOverwrite(path):
  return os.path.isfile(path) and os.path.getsize(path) == 0 and os.access(
      path, os.W_OK)


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

    if os.path.exists(f) and not _IsSafeToOverwrite(f):
      print('A file at path %s already exists' % f, file=sys.stderr)
      return 2

  for f in files:
    _CreateFile(f)


if __name__ == '__main__':
  sys.exit(Main())
