#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tries to compile given code, produces different output depending on success.

This is similar to checks done by ./configure scripts.
"""

from __future__ import print_function

import optparse
import os
import shutil
import subprocess
import sys
import tempfile


def DoMain(argv):
  parser = optparse.OptionParser()
  parser.add_option('--code')
  parser.add_option('--run-linker', action='store_true')
  parser.add_option('--on-success', default='')
  parser.add_option('--on-failure', default='')

  options, args = parser.parse_args(argv)

  if not options.code:
    parser.error('Missing required --code switch.')

  # The environment variable might expand to a string with spaces,
  # e.g. "ccache g++". Convert it to a list suitable for argv.
  cxx = os.environ.get('CXX', 'g++').split()

  tmpdir = tempfile.mkdtemp()
  try:
    cxx_path = os.path.join(tmpdir, 'test.cc')
    with open(cxx_path, 'w') as f:
      f.write(options.code.decode('string-escape'))

    o_path = os.path.join(tmpdir, 'test.o')

    cxx_cmdline = cxx + [cxx_path, '-o', o_path]
    if not options.run_linker:
      cxx_cmdline.append('-c')
    # Pass remaining arguments to the compiler.
    cxx_cmdline += args
    cxx_popen = subprocess.Popen(cxx_cmdline,
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
    cxx_stdout, cxx_stderr = cxx_popen.communicate()
    if cxx_popen.returncode == 0:
      print(options.on_success)
    else:
      print(options.on_failure)
  finally:
    shutil.rmtree(tmpdir)

  return 0


if __name__ == '__main__':
  sys.exit(DoMain(sys.argv[1:]))
