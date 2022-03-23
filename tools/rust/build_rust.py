#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Assembles a Rust toolchain in-tree linked against in-tree LLVM.

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
//tools/clang/scripts/build.py or clang-libs fetched from
//tools/clang/scripts/update.py.

Ideally our build would begin with our own trusted stage0 rustc. As it is
simpler, for now we use an official build.

TODO(https://crbug.com/1245714): Do a proper 3-stage build

'''

import argparse
import collections
import hashlib
import os
import pipes
import shutil
import string
import subprocess
import sys

from pathlib import Path

# Get variables and helpers from Clang update script
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'clang',
                 'scripts'))

from update import (CHROMIUM_DIR, CLANG_REVISION, CLANG_SUB_REVISION,
                    LLVM_BUILD_DIR, GetDefaultHostOs, RmTree, UpdatePackage)
import build

from update_rust import (RUST_REVISION, RUST_SUB_REVISION, STAGE0_JSON_SHA256,
                         GetPackageVersion)

RUST_GIT_URL = 'https://github.com/rust-lang/rust/'

THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
RUST_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-src')
STAGE0_JSON_PATH = os.path.join(RUST_SRC_DIR, 'src', 'stage0.json')
# Download crates.io dependencies to rust-src subdir (rather than $HOME/.cargo)
CARGO_HOME_DIR = os.path.join(RUST_SRC_DIR, 'cargo-home')
RUST_SRC_VERSION_FILE_PATH = os.path.join(RUST_SRC_DIR, 'src', 'version')
RUST_TOOLCHAIN_OUT_DIR = os.path.join(THIRD_PARTY_DIR, 'rust-toolchain')
RUST_TOOLCHAIN_LIB_DIR = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'lib')
VERSION_STAMP_PATH = os.path.join(RUST_TOOLCHAIN_OUT_DIR, 'VERSION')
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
        and RunCommand(['git', 'fetch'], fail_hard=False)
        and RunCommand(['git', 'checkout', commit], fail_hard=False)
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


def VerifyStage0JsonHash():
  hasher = hashlib.sha256()
  with open(STAGE0_JSON_PATH, 'rb') as input:
    hasher.update(input.read())
  actual_hash = hasher.hexdigest()

  if actual_hash == STAGE0_JSON_SHA256:
    return

  print('src/stage0.json hash is different than expected!')
  print('Expected hash: ' + STAGE0_JSON_SHA256)
  print('Actual hash:   ' + actual_hash)
  sys.exit(1)


def Configure(llvm_libs_root):
  # Read the config.toml template file...
  with open(RUST_CONFIG_TEMPLATE_PATH, 'r') as input:
    template = string.Template(input.read())

  subs = {}
  subs['INSTALL_DIR'] = RUST_TOOLCHAIN_OUT_DIR
  subs['LLVM_ROOT'] = llvm_libs_root
  subs['PACKAGE_VERSION'] = GetPackageVersion()

  # ...and apply substitutions, writing to config.toml in Rust tree.
  with open(os.path.join(RUST_SRC_DIR, 'config.toml'), 'w') as output:
    output.write(template.substitute(subs))


def RunXPy(sub, args, gcc_toolchain_path, verbose):
  ''' Run x.py, Rust's build script'''
  clang_path = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang')
  RUSTENV = collections.defaultdict(str, os.environ)
  # Cargo normally stores files in $HOME. Override this.
  RUSTENV['CARGO_HOME'] = CARGO_HOME_DIR
  RUSTENV['AR'] = os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-ar')
  RUSTENV['CC'] = clang_path
  RUSTENV['CXX'] = os.path.join(LLVM_BUILD_DIR, 'bin', 'clang++')
  RUSTENV['LD'] = clang_path
  # We use these flags to avoid linking with the system libstdc++.
  gcc_toolchain_flag = (f'--gcc-toolchain={gcc_toolchain_path}'
                        if gcc_toolchain_path else '')
  # These affect how C/C++ files are compiled, but not Rust libs/exes.
  RUSTENV['CFLAGS'] += f' {gcc_toolchain_flag}'
  RUSTENV['CXXFLAGS'] += f' {gcc_toolchain_flag}'
  RUSTENV['LDFLAGS'] += f' {gcc_toolchain_flag}'
  # These affect how Rust crates are built. A `-Clink-arg=<foo>` arg passes foo
  # to the clang invocation used to link.
  #
  # TODO(https://crbug.com/1281664): remove --no-gc-sections argument.
  # Workaround for a bug causing std::env::args() to return an empty list,
  # making Rust binaries unable to take command line arguments. Fix is landed
  # upstream in LLVM but hasn't rolled into Chromium. Also see:
  # * https://github.com/rust-lang/rust/issues/92181
  # * https://reviews.llvm.org/D116528
  RUSTENV['RUSTFLAGS_BOOTSTRAP'] = (
      f'-Clinker={clang_path} -Clink-arg=-fuse-ld=lld '
      f'-Clink-arg=-Wl,--no-gc-sections -Clink-arg={gcc_toolchain_flag} '
      f'-L native={gcc_toolchain_path}/lib64')
  RUSTENV['RUSTFLAGS_NOT_BOOTSTRAP'] = RUSTENV['RUSTFLAGS_BOOTSTRAP']
  os.chdir(RUST_SRC_DIR)
  cmd = [sys.executable, 'x.py', sub]
  if verbose and verbose > 0:
    cmd.append('-' + verbose * 'v')
  RunCommand(cmd + args, env=RUSTENV)


def main():
  parser = argparse.ArgumentParser(
      description='Build and package Rust toolchain')
  parser.add_argument('-v',
                      '--verbose',
                      action='count',
                      help='run subcommands with verbosity')
  parser.add_argument(
      '--verify-stage0-hash',
      action='store_true',
      help='checkout Rust, verify the stage0 hash, then quit without building. '
      'Will print the actual hash if different than expected.')
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
      help='fetch Clang/LLVM libs and extract into LLVM_BUILD_DIR. Useless '
      'without --use-final-llvm-build-dir.')
  parser.add_argument(
      '--use-final-llvm-build-dir',
      action='store_true',
      help='use libs in LLVM_BUILD_DIR instead of LLVM_BOOTSTRAP_DIR. Useful '
      'with --fetch-llvm-libs for local builds.')
  parser.add_argument(
      '--run-xpy',
      action='store_true',
      help='run x.py command in configured Rust checkout. Quits after running '
      'specified command, skipping all normal build steps. For debugging. '
      'Running x.py directly will not set the appropriate env variables nor '
      'update config.toml')
  args, rest = parser.parse_known_args()

  # Get the LLVM root for libs. We use LLVM_BUILD_DIR tools either way.
  #
  # TODO(https://crbug.com/1245714): use LTO libs from LLVM_BUILD_DIR for
  # stage 2+.
  if args.use_final_llvm_build_dir:
    llvm_libs_root = LLVM_BUILD_DIR
  else:
    llvm_libs_root = build.LLVM_BOOTSTRAP_DIR

  if not args.skip_checkout:
    CheckoutRust(RUST_REVISION, RUST_SRC_DIR)

  VerifyStage0JsonHash()
  if args.verify_stage0_hash:
    # The above function exits and prints the actual hash if verification failed
    # so we just quit here; if we reach this point, the hash is valid.
    return 0

  if args.fetch_llvm_libs:
    UpdatePackage('clang-libs', GetDefaultHostOs())

  # Fetch GCC package to build against same libstdc++ as Clang. This function
  # will only download it if necessary.
  args.gcc_toolchain = None
  build.MaybeDownloadHostGcc(args)

  # Set up config.toml in Rust source tree to configure build.
  Configure(llvm_libs_root)

  if args.run_xpy:
    if rest[0] == '--':
      rest = rest[1:]
    RunXPy(rest[0], rest[1:], args.gcc_toolchain, args.verbose)
    return 0
  else:
    assert not rest

  # Delete vendored sources and .cargo subdir. Otherwise when updating an
  # existing checkout, vendored sources will not be re-fetched leaving deps out
  # of date.
  for dir in [os.path.join(RUST_SRC_DIR, d) for d in ['vendor', '.cargo']]:
    if os.path.exists(dir):
      shutil.rmtree(dir)

  if not args.skip_clean:
    print('Cleaning build artifacts...')
    RunXPy('clean', [], args.gcc_toolchain, args.verbose)

  if not args.skip_test:
    print('Running stage 2 tests...')
    # Run a subset of tests. Tell x.py to keep the rustc we already built.
    RunXPy('test',
           ['--stage', '2', 'library/std', 'src/test/codegen', 'src/test/ui'],
           args.gcc_toolchain, args.verbose)

  targets = [
      'library/proc_macro', 'library/std', 'src/tools/cargo',
      'src/tools/clippy', 'src/tools/rustfmt'
  ]

  # Build stage 2 compiler, tools, and libraries. This should reuse earlier
  # stages from the test command (if run).
  print('Building stage 2 artifacts...')
  RunXPy('build', ['--stage', '2'] + targets, args.gcc_toolchain, args.verbose)

  if not args.skip_install:
    print(f'Installing to {RUST_TOOLCHAIN_OUT_DIR} ...')
    # Clean output directory.
    if os.path.exists(RUST_TOOLCHAIN_OUT_DIR):
      shutil.rmtree(RUST_TOOLCHAIN_OUT_DIR)

    RunXPy('install', DISTRIBUTION_ARTIFACTS, args.gcc_toolchain, args.verbose)

    # Write expected `rustc --version` string to our toolchain directory.
    with open(RUST_SRC_VERSION_FILE_PATH) as version_file:
      rust_version = version_file.readline().rstrip()
    with open(VERSION_STAMP_PATH, 'w') as stamp:
      stamp.write('rustc %s-dev (%s chromium)\n' %
                  (rust_version, GetPackageVersion()))

  return 0


if __name__ == '__main__':
  sys.exit(main())
