#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds rustc in-tree, linked against an in-tree build of LLVM.

!!! DO NOT USE IN PRODUCTION
Builds a Rust toolchain bootstrapped from an untrusted rustc build.

Rust has an official boostrapping build. At a high level:
1. A "stage 0" Rust is downloaded from Rust's official server. This
   is one major release before the version being built. E.g. if building trunk
   the latest beta is downloaded. If building stable 1.57.0, stage0 is stable
   1.56.1.
2. Stage 0 libstd is built. This is different than the libstd downloaded above.
3. Stage 1 rustc is built with rustc from (1) and libstd from (2)
2. Stage 1 libstd is built with stage 1 rustc. Later artifacts built with
   stage 1 rustc are built with stage 1 libstd.

Further stages are possible and continue as expected. Additionally, the build
can be extensively customized. See for details:
https://rustc-dev-guide.rust-lang.org/building/bootstrapping.html

This script clones the Rust repository, checks it out to a defined revision,
then builds stage 1 rustc and libstd using the LLVM build from
//tools/clang/scripts/build.py.

Ideally our build would begin with our own trusted stage0 rustc. As it is
simpler, for now we use an official build.

TODO(https://crbug.com/1245714): Do a proper 3-stage build

'''

import argparse
import os
import sys
import pipes
import string
import subprocess

from pathlib import Path

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))
from update import (CHROMIUM_DIR, CLANG_REVISION, CLANG_SUB_REVISION,
                    LLVM_BUILD_DIR, GetDefaultHostOs, RmTree, UpdatePackage)

# Trunk on 1/26/2021
RUST_REVISION = 'a7f3757'
RUST_SUB_REVISION = 1

PACKAGE_VERSION = '%s-%s-%s-%s' % (RUST_REVISION, RUST_SUB_REVISION,
                                   CLANG_REVISION, CLANG_SUB_REVISION)

RUST_GIT_URL = 'https://github.com/rust-lang/rust/'

THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-src')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
RUST_CONFIG_TEMPLATE_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'config.toml.template')

# Desired tools and libraries in our Rust toolchain.
DISTRIBUTION_ARTIFACTS = [
    'cargo', 'clippy', 'library/std', 'rust-analyzer', 'rustfmt', 'src/librustc'
]


def RunCommand(command, env=None, fail_hard=True):
  print('Running', command)
  if subprocess.run(command, env=env,
                    shell=sys.platform == 'win32').returncode == 0:
    return True
  print('Failed.')
  if fail_hard:
    sys.exit(1)
  return False


def CheckoutRust(commit, dir):
  # Submodules we must update early since bootstrap wants them before it starts
  # managing them.
  force_update_submodules = [
      'src/tools/rust-analyzer', 'compiler/rustc_codegen_cranelift'
  ]

  # Shared between first checkout and subsequent updates.
  def UpdateSubmodules():
    return RunCommand(['git', 'submodule', 'update', '--init', '--recursive'] +
                      force_update_submodules,
                      fail_hard=False)

  # Try updating the current repo if it exists and has no local diff.
  if os.path.isdir(dir):
    os.chdir(dir)
    # git diff-index --quiet returns success when there is no diff.
    # Also check that the first commit is reachable.
    if (RunCommand(['git', 'diff-index', '--quiet', 'HEAD'], fail_hard=False)
        and UpdateSubmodules()):
      return

    # If we can't use the current repo, delete it.
    os.chdir(CHROMIUM_DIR)  # Can't remove dir if we're in it.
    print('Removing %s.' % dir)
    RmTree(dir)

  clone_cmd = ['git', 'clone', RUST_GIT_URL, dir]

  if RunCommand(clone_cmd, fail_hard=False):
    os.chdir(dir)
    if (RunCommand(['git', 'checkout', commit], fail_hard=False)
        and UpdateSubmodules()):
      return

  print('CheckoutRust failed.')
  sys.exit(1)


def Configure():
  # Read the config.toml template file...
  with open(RUST_CONFIG_TEMPLATE_PATH, 'r') as input:
    template = string.Template(input.read())

  subs = {}
  subs['INSTALL_DIR'] = RUST_TOOLCHAIN_OUT_DIR
  subs['LLVM_ROOT'] = LLVM_BUILD_DIR

  # ...and apply substitutions, writing to config.toml in Rust tree.
  with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
    output.write(template.substitute(subs))


def RunXPy(sub, args, verbose=False):
  ''' Run x.py, Rust's build script'''
  clang_path = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')
  RUSTENV = os.environ
  RUSTENV['AR'] = os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-ar')
  RUSTENV['CC'] = clang_path
  RUSTENV['CXX'] = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang++')
  RUSTENV['LD'] = clang_path
  # TODO(https://crbug.com/1281664): remove --no-gc-sections argument.
  # Workaround for a bug causing std::env::args() to return an empty list,
  # making Rust binaries unable to take command line arguments. Fix is landed
  # upstream in LLVM but hasn't rolled into Chromium. Also see:
  # * https://github.com/rust-lang/rust/issues/92181
  # * https://reviews.llvm.org/D116528
  RUSTENV[
      'RUSTFLAGS_BOOTSTRAP'] = '-Clinker=%s -Clink-arg=-fuse-ld=lld -Clink-arg=-Wl,--no-gc-sections' % clang_path
  RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] = RUSTENV['RUSTFLAGS_BOOTSTRAP']
  os.chdir(RUST_SRC_DIR)
  cmd = [sys.executable, 'x.py', sub]
  if verbose:
    cmd.append('-v')
  RunCommand(cmd + args)


def main():
  parser = argparse.ArgumentParser(
      description='Build and package Rust toolchain')
  parser.add_argument('-v',
                      '--verbose',
                      action='store_true',
                      help='run subcommands with verbosity')
  parser.add_argument(
      '--skip-checkout',
      action='store_true',
      help='skip Rust git checkout. Useful for trying local changes')
  parser.add_argument('--skip-clean',
                      action='store_true',
                      help='skip x.py clean step')
  parser.add_argument('--skip-test',
                      action='store_true',
                      help='skip running rustc and libstd tests')
  parser.add_argument('--skip-install',
                      action='store_true',
                      help='do not install to RUST_TOOLCHAIN_OUT_DIR')
  parser.add_argument(
      '--fetch-llvm-libs',
      action='store_true',
      help='fetch Clang/LLVM libs and extract into LLVM_BUILD_DIR')
  args = parser.parse_args()

  if args.fetch_llvm_libs:
    UpdatePackage('clang-libs', GetDefaultHostOs())

  if not args.skip_checkout:
    CheckoutRust(RUST_REVISION, RUST_SRC_DIR)

  # Set up config.toml in Rust source tree to configure build.
  Configure()

  if not args.skip_clean:
    RunXPy('clean', [], verbose=args.verbose)

  targets = [
      'library/proc_macro', 'library/std', 'src/tools/cargo',
      'src/tools/clippy', 'src/tools/rustfmt'
  ]

  # Build stage 1 compiler and tools.
  RunXPy('build', ['--stage', '1'] + targets, verbose=args.verbose)

  if not args.skip_test:
    # Run a subset of tests. Tell x.py to keep the rustc we already built.
    RunXPy('test', [
        '--stage', '1', '--keep-stage', '1', 'library/std', 'src/test/codegen',
        'src/test/ui'
    ],
           verbose=args.verbose)

  if not args.skip_install:
    RunXPy('install',
           ['--stage', '1', '--keep-stage', '1'] + DISTRIBUTION_ARTIFACTS)


if __name__ == '__main__':
  sys.exit(main())
