#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Dumps a list of files with static initializers. Use with release builds.

Usage:
  tools/mac/dump-static-initializers.py out/Release/Chromium\ Framework.framework.dSYM/Contents/Resources/DWARF/Chromium\ Framework

Do NOT use mac_strip_release=0 or component=shared_library if you want to use
this script.

This is meant to be used on a dSYM file. If only an unstripped executable is
present, use show_mod_init_func.py.
"""

from __future__ import print_function

import optparse
import re
import subprocess
import sys

# Matches for example:
# [     1] 000001ca 64 (N_SO         ) 00     0000   0000000000000000 'test.cc'
dsymutil_file_re = re.compile("N_SO.*'([^']*)'")

# Matches for example:
# [     2] 000001d2 66 (N_OSO        ) 00     0001   000000004ed856a0 '/Volumes/MacintoshHD2/src/chrome-git/src/test.o'
dsymutil_o_file_re = re.compile("N_OSO.*'([^']*)'")

# Matches for example:
# [     8] 00000233 24 (N_FUN        ) 01     0000   0000000000001b40 '__GLOBAL__I_s'
# [185989] 00dc69ef 26 (N_STSYM      ) 02     0000   00000000022e2290 '__GLOBAL__I_a'
dsymutil_re = re.compile(r"(?:N_FUN|N_STSYM).*\s[0-9a-f]*\s'__GLOBAL__I_")

def ParseDsymutil(binary):
  """Given a binary, prints source and object filenames for files with
  static initializers.
  """

  child = subprocess.Popen(['tools/clang/dsymutil/bin/dsymutil', '-s', binary],
     stdout=subprocess.PIPE)
  for line in child.stdout:
    file_match = dsymutil_file_re.search(line)
    if file_match:
      current_filename = file_match.group(1)
    else:
      o_file_match = dsymutil_o_file_re.search(line)
      if o_file_match:
        current_o_filename = o_file_match.group(1)
      else:
        match = dsymutil_re.search(line)
        if match:
          print(current_filename)
          print(current_o_filename)
          print()


def main():
  parser = optparse.OptionParser(usage='%prog filename')
  opts, args = parser.parse_args()
  if len(args) != 1:
    parser.error('missing filename argument')
    return 1
  binary = args[0]

  ParseDsymutil(binary)
  return 0


if '__main__' == __name__:
  sys.exit(main())
