#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used by chrome_tests.gypi's js2webui action to maintain the
argument lists and to generate inlinable tests.
"""

from __future__ import print_function

import json
import optparse
import os
import subprocess
import sys
import shutil


def HasSameContent(filename, content):
  '''Returns true if the given file is readable and has the given content.'''
  try:
    with open(filename) as file:
      return file.read() == content
  except:
    # Ignore all errors and fall back on a safe bet.
    return False


def main ():
  parser = optparse.OptionParser()
  parser.set_usage(
      "%prog v8_shell mock.js test_api.js js2webui.js "
      "testtype inputfile srcrootdir cxxoutfile jsoutfile")
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-n', '--impotent', action='store_true',
                    help="don't execute; just print (as if verbose)")
  parser.add_option('--deps_js', action="store",
                    help=("Path to deps.js for dependency resolution, " +
                          "optional."))
  parser.add_option('--external', action='store',
                    help="Load V8's initial snapshot from external files (y/n)")
  (opts, args) = parser.parse_args()

  if len(args) != 9:
    parser.error('all arguments are required.')
  (v8_shell, mock_js, test_api, js2webui, test_type,
      inputfile, srcrootdir, cxxoutfile, jsoutfile) = args
  cmd = [v8_shell]
  arguments = [js2webui, inputfile, srcrootdir, opts.deps_js,
               cxxoutfile, test_type]
  cmd.extend(['-e', "arguments=" + json.dumps(arguments), mock_js,
         test_api, js2webui])
  if opts.verbose or opts.impotent:
    print(cmd)
  if not opts.impotent:
    try:
      p = subprocess.Popen(
          cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
      out, err = p.communicate()
      if p.returncode != 0:
        sys.stderr.write(out + err);
        return 1
      if not HasSameContent(cxxoutfile, out):
        with open(cxxoutfile, 'wb') as f:
          f.write(out)
      shutil.copyfile(inputfile, jsoutfile)
    except Exception, ex:
      if os.path.exists(cxxoutfile):
        os.remove(cxxoutfile)
      if os.path.exists(jsoutfile):
        os.remove(jsoutfile)
      raise


if __name__ == '__main__':
  sys.exit(main())
