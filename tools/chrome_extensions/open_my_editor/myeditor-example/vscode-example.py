#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This is a wrapper script to start your editor. You also need to add this script
to your path.
"""

import argparse
import os
import sh

CHROMIUM_ROOT = os.environ['HOME'] + "/workspace/chromium/src"

os.chdir(CHROMIUM_ROOT)

###### Dont change this part. BEGIN #####
parser = argparse.ArgumentParser()
parser.add_argument("-f", "--filepath", help="Filepath.")
parser.add_argument("-l", "--line", type=int, help="Line number.")
parser.add_argument("-m", "--multifilepath", help="Multi Filepath.")
args = parser.parse_args()
###### Dont change this part. END   #####

# Start a vscode instance.
sh.code(".")

# Open one file with line number.
if args.filepath != None:
  sh.code("-g", args.filepath + ":" + str(args.line))
# Open multiple files.
else:
  sh.code(args.multifilepath.split(",,"))
