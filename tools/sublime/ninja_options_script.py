#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage within SublimeClang:
#   "sublimeclang_options_script": "python
#       ${project_path}/src/tools/sublime/ninja_options_script.py \
#       -d '/path/to/depot_tools'"
#
#
# NOTE: ${project_path} expands to the directory of the Sublime project file,
# and SublimeClang passes the absolute file path to the current file as an
# additional argument. You should change the -d argument to point to your
# depot_tools directory.

from __future__ import print_function

import imp
import optparse
import os
import shlex

ycm_module_path = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
    '../vim/chromium.ycm_extra_conf.py'))
ycm_extra_conf = imp.load_source('ycm_extra_conf', ycm_module_path)

def main():
  usage = "usage: %prog [options] file"
  parser = optparse.OptionParser(usage)
  parser.add_option("-d", "--depot_tools", dest="depot_path",
                  help="path to depot_tools")
  (options, args) = parser.parse_args()
  if options.depot_path:
    os.environ["PATH"] += ":%s" % options.depot_path
  if len(args) != 1:
    parser.error("incorrect number of arguments")

  path = os.path.realpath(args[0])
  results = ycm_extra_conf.FlagsForFile(path)

  for flag in results['flags']:
    # The sublimeclang plugin expects to parse its input with shlex.
    # Defines and include path names may have spaces or quotes.
    print(shlex.quote(flag))


if __name__ == "__main__":
  main()
