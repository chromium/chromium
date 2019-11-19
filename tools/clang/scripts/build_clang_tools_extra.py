#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script for fetching LLVM monorepo and building clang-tools-extra binaries.

Example: build clangd and clangd-indexer

   tools/clang/scripts/build_clang_tools_extra.py --fetch out/Release clangd \
       clangd-indexer
"""

from __future__ import print_function

import argparse
import errno
import os
import subprocess
import sys
import update


def GetCheckoutDir(out_dir):
  """Returns absolute path to the checked-out llvm repo."""
  return os.path.join(out_dir, 'tools', 'clang', 'third_party', 'llvm')


def GetBuildDir(out_dir):
  return os.path.join(GetCheckoutDir(out_dir), 'build')


def CreateDirIfNotExists(dir):
  if not os.path.exists(dir):
    os.makedirs(dir)


def FetchLLVM(checkout_dir, revision):
  """Clone llvm repo into |out_dir| or update if it already exists."""
  CreateDirIfNotExists(os.path.dirname(checkout_dir))

  try:
    # First, try to clone the repo.
    args = [
        'git',
        'clone',
        'https://github.com/llvm/llvm-project.git',
        checkout_dir,
    ]
    subprocess.check_call(args, shell=sys.platform == 'win32')
  except subprocess.CalledProcessError:
    # Otherwise, try to update it.
    print('-- Attempting to update existing repo')
    args = ['git', 'pull', '--rebase', 'origin', 'master']
    subprocess.check_call(args, cwd=checkout_dir)
  if revision:
    args = ['git', 'checkout', revision]
    subprocess.check_call(args, cwd=checkout_dir)


def BuildTargets(build_dir, targets):
  """Build targets from llvm repo at |build_dir|."""
  CreateDirIfNotExists(build_dir)

  # From that dir, run cmake
  cmake_args = [
      'cmake',
      '-GNinja',
      '-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra',
      '-DCMAKE_BUILD_TYPE=Release',
      '-DLLVM_ENABLE_ASSERTIONS=On',
      '../llvm',
  ]
  subprocess.check_call(cmake_args, cwd=build_dir)

  ninja_commands = ['ninja'] + targets
  subprocess.check_call(ninja_commands, cwd=build_dir)


def main():
  parser = argparse.ArgumentParser(description='Build clang_tools_extra.')
  parser.add_argument('--fetch', action='store_true', help='fetch LLVM source')
  parser.add_argument(
      '--revision', help='LLVM revision to use', default=update.CLANG_REVISION)
  parser.add_argument('OUT_DIR', help='where we put the LLVM source repository')
  parser.add_argument('TARGETS', nargs='+', help='targets being built')
  args = parser.parse_args()

  if args.fetch:
    print('Fetching LLVM source')
    FetchLLVM(GetCheckoutDir(args.OUT_DIR), args.revision)

  print('Building targets: %s' % ', '.join(args.TARGETS))
  BuildTargets(GetBuildDir(args.OUT_DIR), args.TARGETS)


if __name__ == '__main__':
  sys.exit(main())
