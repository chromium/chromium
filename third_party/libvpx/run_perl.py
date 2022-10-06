#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"This script is used to run a perl script."

import optparse
import subprocess
import sys

parser = optparse.OptionParser()
parser.description = __doc__
parser.add_option('-s', '--script', help='path to a perl script.')
parser.add_option('-i', '--input', help='file passed to stdin.')
parser.add_option('-o', '--output', help='file saved from stdout.')


options, args = parser.parse_args()
if (not options.script or not options.input or not options.output):
  parser.error('Must specify arguments for script, input and output.')
  sys.exit(1)

with open(options.output, 'w') as fo, open(options.input, 'r') as fi:
  subprocess.check_call(['perl', options.script], stdout=fo, stdin=fi)

sys.exit(0)
