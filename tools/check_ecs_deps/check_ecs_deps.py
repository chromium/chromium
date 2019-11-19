#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Verifies that builds of the embedded content_shell do not included
unnecessary dependencies.'''

from __future__ import print_function

import os
import re
import string
import subprocess
import sys
import optparse

kUndesiredLibraryList = [
  'libX11',
  'libXau',
  'libXcomposite',
  'libXcursor',
  'libXdamage',
  'libXdmcp',
  'libXext',
  'libXfixes',
  'libXi',
  'libXrandr',
  'libXrender',
  'libXtst',
  'libasound',
  'libcairo',
  'libdbus',
  'libffi',
  'libgconf',
  'libgio',
  'libglib',
  'libgmodule',
  'libgobject',
  'libpango',
  'libpcre',
  'libpixman',
  'libpng',
  'libselinux',
  'libudev',
  'libxcb',
]

kAllowedLibraryList = [
  # Toolchain libraries (gcc/glibc)
  'ld-linux',
  'libc',
  'libdl',
  'libgcc_s',
  'libm',
  'libpthread',
  'libresolv',
  'librt',
  'libstdc++',
  'linux-vdso',

  # Needed for default ozone platforms
  'libdrm',

  # NSS & NSPR
  'libnss3',
  'libnssutil3',
  'libnspr4',
  'libplc4',
  'libplds4',
  'libsmime3',

  # OpenSSL
  'libcrypto',

  # Miscellaneous
  'libcap',
  'libexpat',
  'libfontconfig',
  'libz',
]

binary_target = 'content_shell'

def stdmsg(_final, errors):
  if errors:
    for message in errors:
      print(message)


def bbmsg(final, errors):
  if errors:
    for message in errors:
      print('@@@STEP_TEXT@%s@@@' % message)
  if final:
    print('\n@@@STEP_%s@@@' % final)


def _main():
  output = {
    'message': lambda x: stdmsg(None, x),
    'fail': lambda x: stdmsg('FAILED', x),
    'warn': lambda x: stdmsg('WARNING', x),
    'abend': lambda x: stdmsg('FAILED', x),
    'ok': lambda x: stdmsg('SUCCESS', x),
    'verbose': lambda x: None,
  }

  parser = optparse.OptionParser(
      "usage: %prog -b <dir> --target <Debug|Release>")
  parser.add_option("", "--annotate", dest='annotate', action='store_true',
      default=False, help="include buildbot annotations in output")
  parser.add_option("", "--noannotate", dest='annotate', action='store_false')
  parser.add_option("-b", "--build-dir",
                    help="the location of the compiler output")
  parser.add_option("--target", help="Debug or Release")
  parser.add_option('-v', '--verbose', default=False, action='store_true')

  options, args = parser.parse_args()
  if args:
    parser.usage()
    return -1

  # Bake target into build_dir.
  if options.target and options.build_dir:
    assert (options.target !=
            os.path.basename(os.path.dirname(options.build_dir)))
    options.build_dir = os.path.join(os.path.abspath(options.build_dir),
                                     options.target)

  if options.build_dir != None:
    build_dir = os.path.abspath(options.build_dir)
  else:
    build_dir = os.getcwd()

  target = os.path.join(build_dir, binary_target)

  if options.annotate:
    output.update({
      'message': lambda x: bbmsg(None, x),
      'fail': lambda x: bbmsg('FAILURE', x),
      'warn': lambda x: bbmsg('WARNINGS', x),
      'abend': lambda x: bbmsg('EXCEPTIONS', x),
      'ok': lambda x: bbmsg(None, x),
    })

  if options.verbose:
    output['verbose'] = lambda x: stdmsg(None, x)

  forbidden_regexp = re.compile(string.join(map(re.escape,
                                                kUndesiredLibraryList), '|'))
  mapping_regexp = re.compile(r"\s*([^/]*) => (.*)")
  blessed_regexp = re.compile(r"(%s)[-0-9.]*\.so" % string.join(map(re.escape,
      kAllowedLibraryList), '|'))
  built_regexp = re.compile(re.escape(build_dir + os.sep))

  success = 0
  warning = 0

  p = subprocess.Popen(['ldd', target], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  out, err = p.communicate()

  if err != '':
    output['abend']([
      'Failed to execute ldd to analyze dependencies for ' + target + ':',
      '    ' + err,
    ])
    return 1

  if out == '':
    output['abend']([
      'No output to scan for forbidden dependencies.'
    ])
    return 1

  success = 1
  deps = string.split(out, '\n')
  for d in deps:
    libmatch = mapping_regexp.match(d)
    if libmatch:
      lib = libmatch.group(1)
      source = libmatch.group(2)
      if forbidden_regexp.search(lib):
        success = 0
        output['message'](['Forbidden library: ' + lib])
      elif built_regexp.match(source):
        output['verbose'](['Built library: ' + lib])
      elif blessed_regexp.match(lib):
        output['verbose'](['Blessed library: ' + lib])
      else:
        warning = 1
        output['message'](['Unexpected library: ' + lib])

  if success == 1:
    if warning == 1:
      output['warn'](None)
    else:
      output['ok'](None)
    return 0
  else:
    output['fail'](None)
    return 1

if __name__ == "__main__":
  # handle arguments...
  # do something reasonable if not run with one...
  sys.exit(_main())
