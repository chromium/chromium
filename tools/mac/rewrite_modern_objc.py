#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs clang's "modern objective-c" rewriter on chrome code.
Does the same as Xcode's Edit->Convert->To Modern Objective-C Syntax.

Note that this just runs compile commands and doesn't look at build
dependencies, i.e. it doesn't make sure generated headers exist.  It also
requires goma to be disabled.  Suggested workflow: Build the target you want
to convert locally with goma to create generated headers, then disable goma,
re-run gn, and then run this script.

Since Chrome's clang disables the rewriter, to run this you will need to
build ToT clang with `-DCLANG_ENABLE_ARCMT` and (temporarily) add the following
to your Chromium build args:
clang_base_path = /path/to/clang
clang_use_chrome_plugins = false
"""

from __future__ import print_function

import argparse
import glob
import json
import math
import os
import shlex
import subprocess
import sys

def main():
  # As far as I can tell, clang's ObjC rewriter can't do in-place rewriting
  # (the ARC rewriter can).  libclang exposes functions for parsing the remap
  # file, but doing that manually in python seems a lot easier.

  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('builddir', help='build directory, e.g. out/gn')
  parser.add_argument('substr', default='', nargs='?',
                      help='source dir part, eg chrome/browser/ui/cocoa')
  args = parser.parse_args()

  rewrite_dir = os.path.abspath(
      os.path.join(args.builddir, 'rewrite_modern_objc'))
  try:
    os.mkdir(rewrite_dir)
  except OSError:
    pass

  remap_file = os.path.join(rewrite_dir, 'remap')
  try:
    # Remove remap files from prior runs.
    os.remove(remap_file)
  except OSError:
    pass

  # The basic idea is to call clang's objcmt rewriter for each source file.
  # The rewriter writes a "remap" file containing N times 3 lines:
  # Name of an original source file, the original file's timestamp
  # at rewriting time, and the name of a temp file containing the rewritten
  # contents.
  # The rewriter gets confused if several instances run in parallel.  We could
  # be fancy and have num_cpus rewrite dirs and combine their contents in the
  # end, but for now just run the rewrites serially.

  # First, ask ninja for the compile commands of all .m and .mm files.
  compdb = subprocess.check_output(
      ['ninja', '-C', args.builddir, '-t', 'compdb', 'objc', 'objcxx'])

  for cmd in json.loads(compdb):
    objc_file = cmd['file']
    if args.substr not in objc_file:
      continue
    clang_cmd = cmd['command']

    had_error = False
    if 'gomacc' in clang_cmd:
      print('need builddir with use_goma not set', file=sys.stderr)
      had_error = True
    if 'jumbo' in clang_cmd:
      print('need builddir with use_jumbo_build not set', file=sys.stderr)
      had_error = True
    if 'precompile.h-m' in clang_cmd:
      print(
          'need builddir with enable_precompiled_headers=false',
          file=sys.stderr)
      had_error = True
    if had_error:
      sys.exit(1)

    # Ninja creates the directory containing the build output, but we
    # don't run ninja, so we need to do that ourselves.
    split_cmd = shlex.split(clang_cmd)
    o_index = split_cmd.index('-o')
    assert o_index != -1
    try:
      os.makedirs(os.path.dirname(split_cmd[o_index + 1]))
    except OSError:
      pass

    # Add flags to tell clang to do the rewriting.
    # Passing "-ccc-objcmt-migrate dir" doesn't give us control over each
    # individual setting, so use the Xclang flags.  The individual flags are at
    # http://llvm-cs.pcc.me.uk/tools/clang/include/clang/Driver/Options.td#291
    # Note that -objcmt-migrate-all maps to ObjCMT_MigrateDecls in
    # http://llvm-cs.pcc.me.uk/tools/clang/lib/Frontend/CompilerInvocation.cpp#1479
    # which is not quite all the options:
    # http://llvm-cs.pcc.me.uk/tools/clang/include/clang/Frontend/FrontendOptions.h#248

    flags = ['-Xclang', '-mt-migrate-directory', '-Xclang', rewrite_dir]
    flags += ['-Xclang', '-objcmt-migrate-subscripting' ]
    flags += ['-Xclang', '-objcmt-migrate-literals' ]
    #flags += ['-Xclang', '-objcmt-returns-innerpointer-property'] # buggy
    #flags += ['-Xclang', '-objcmt-migrate-property-dot-syntax'] # do not want
    # objcmt-migrate-all is the same as the flags following it here (it does
    # not include the flags listed above it).
    # Probably don't want ns-nonatomic-iosonly (or atomic-property), so we
    # can't use migrate-alll which includes that, and have to manually set the
    # bits of migrate-all we do want.
    #flags += ['-Xclang', '-objcmt-migrate-all']
    #flags += ['-Xclang', '-objcmt-migrate-property']  # not sure if want
    flags += ['-Xclang', '-objcmt-migrate-annotation']
    flags += ['-Xclang', '-objcmt-migrate-instancetype']
    flags += ['-Xclang', '-objcmt-migrate-ns-macros']
    #flags += ['-Xclang', '-objcmt-migrate-protocol-conformance'] # buggy
    #flags += ['-Xclang', '-objcmt-atomic-property']  # not sure if want
    #flags += ['-Xclang', '-objcmt-ns-nonatomic-iosonly']  # not sure if want
    # Want, but needs careful manual review, and doesn't find everything:
    #flags += ['-Xclang', '-objcmt-migrate-designated-init']
    clang_cmd += ' ' + ' '.join(flags)

    print(objc_file)
    subprocess.check_call(clang_cmd, shell=True, cwd=cmd['directory'])

  if not os.path.exists(remap_file):
    print('no changes')
    return

  # Done with rewriting. Now the read the above-described 'remap' file and
  # copy modified files over the originals.
  remap = open(remap_file).readlines()
  for i in range(0, len(remap), 3):
    infile, mtime, outfile = map(str.strip, remap[i:i+3])
    if args.substr not in infile:
      # Ignore rewritten header files not containing args.substr too.
      continue
    if math.trunc(os.path.getmtime(infile)) != int(mtime):
      print('%s was modified since rewriting; exiting' % infile)
      sys.exit(1)
    os.rename(outfile, infile)  # Copy rewritten file over.

  print('all done. commit, run `git cl format`, commit again, and upload!')


if __name__ == '__main__':
  main()
