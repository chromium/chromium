#! /usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Linker wrapper that performs distributed ThinLTO on Reclient.
#
# Usage: Pass the original link command as parameters to this script.
# E.g. original: clang++ -o foo foo.o
# Becomes: remote_ld clang++ -o foo foo.o

import os
import re
import sys

import remote_link


class RemoteLinkUnix(remote_link.RemoteLinkBase):
  # Target-platform-specific constants.
  WL = '-Wl,'
  TLTO = '-plugin-opt=thinlto'
  SEP = '='
  DATA_SECTIONS = '-fdata-sections'
  FUNCTION_SECTIONS = '-ffunction-sections'
  GROUP_RE = re.compile(WL + '--(?:end|start)-group')
  MACHINE_RE = re.compile('-m([0-9]+)')
  OBJ_PATH = '-plugin-opt=obj-path' + SEP
  OBJ_SUFFIX = '.o'
  PREFIX_REPLACE = TLTO + '-prefix-replace' + SEP
  XIR = '-x ir '

  ALLOWLIST = {
      'chrome',
  }

  def analyze_args(self, args, *posargs, **kwargs):
    # TODO(crbug.com/40113922): Builds are unreliable when all targets use
    # distributed ThinLTO, so we only enable it for some targets.
    # For other targets, we fall back to local ThinLTO. We must use ThinLTO
    # because we build with -fsplit-lto-unit, which requires code generation
    # be done for each object and target.
    # Returning None from this function causes the original, non-distributed
    # linker command to be invoked.
    if args.output is None:
      return None
    if not (args.allowlist or os.path.basename(args.output) in self.ALLOWLIST):
      return None
    return super(RemoteLinkUnix, self).analyze_args(args, *posargs, **kwargs)

  def process_output_param(self, args, i):
    """
    If args[i] is a parameter that specifies the output file,
    returns (output_name, new_i). Else, returns (None, new_i).
    """
    if args[i] == '-o':
      return (os.path.normpath(args[i + 1]), i + 2)
    else:
      return (None, i + 1)


if __name__ == '__main__':
  sys.exit(RemoteLinkUnix().main(sys.argv))
