#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Builds the Crubit tool.

!!! DO NOT USE IN PRODUCTION
Builds the Crubit tool (an experiment for Rust/C++ FFI bindings generation).

This script clones the Crubit repository, checks it out to a defined revision,
and then uses Bazel to build Crubit.
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

from update import (CLANG_REVISION, CLANG_SUB_REVISION, LLVM_BUILD_DIR)
from build import LLVM_BOOTSTRAP_INSTALL_DIR

from update_rust import (CHROMIUM_DIR, RUST_REVISION, RUST_SUB_REVISION,
                         STAGE0_JSON_SHA256, THIRD_PARTY_DIR,
                         GetPackageVersion)

# Trunk on 2022-06-13.
CRUBIT_REVISION = '0a25665ed0df6d4f067bfd5855be1c24d2df3f6c'
CRUBIT_SUB_REVISION = 1

THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
CRUBIT_SRC_DIR = os.path.join(THIRD_PARTY_DIR, 'crubit')


def RunCommand(command, env=None, cwd=None, fail_hard=True):
    print('Running', command)
    if subprocess.run(command, env=env, cwd=cwd,
                      shell=sys.platform == 'win32').returncode == 0:
        return True
    print('Failed.')
    if fail_hard:
        raise RuntimeError(f"Failed to run {command}")
    return False


def CheckoutCrubit(commit, dir):
    """Checkout the Crubit repo at a certain git commit in dir. Any local
  modifications in dir will be lost."""

    print('Checking out crubit repo %s into %s' % (commit, dir))

    # Try updating the current repo if it exists and has no local diff.
    if os.path.isdir(dir):
        os.chdir(dir)
        # git diff-index --quiet returns success when there is no diff.
        # Also check that the first commit is reachable.
        if (RunCommand(['git', 'diff-index', '--quiet', 'HEAD'],
                       fail_hard=False)
                and RunCommand(['git', 'fetch'], fail_hard=False)
                and RunCommand(['git', 'checkout', commit], fail_hard=False)):
            return

        # If we can't use the current repo, delete it.
        os.chdir(CHROMIUM_DIR)  # Can't remove dir if we're in it.
        print('Removing %s.' % dir)
        RmTree(dir)

    clone_cmd = ['git', 'clone', 'https://github.com/google/crubit.git', dir]

    if RunCommand(clone_cmd, fail_hard=False):
        os.chdir(dir)
        if RunCommand(['git', 'checkout', commit], fail_hard=False):
            return

    print('CheckoutCrubit failed.')
    sys.exit(1)


def BuildCrubit():
    # TODO(https://crbug.com/1337346): Use locally built Rust instead of having
    # Bazel always download the whole Rust toolchain from the internet.
    # TODO(https://crbug.com/1337348): Use crates from chromium/src/third_party/rust.
    # TODO(https://crbug.com/1338217): Don't use system-wide C++ stdlib.
    env = {"LLVM_INSTALL_PATH": LLVM_BOOTSTRAP_INSTALL_DIR}
    args = ["bazel", "build", "rs_bindings_from_cc:rs_bindings_from_cc_impl"]

    # CC is set via `--repo_env` rather than via `env` to ensure that we
    # override the defaults from `crubit/.bazelrc`.
    #
    # Note that we use `bin/clang` from `LLVM_BUILD_DIR`, but depend on headers
    # and libraries from `LLVM_BOOTSTRAP_INSTALL_DIR`.  The former helps ensure
    # that we use the same compiler as the final one used elsewhere in Chromium.
    # The latter is needed, because the headers+libraries are not available
    # anywhere else.
    clang_path = os.path.join(LLVM_BUILD_DIR, "bin", "clang")
    args += [
        "--repo_env=CC=",  # Unset/ignore old values from the environment.
        "--repo_env=CXX=",
        "--repo_env=LD=",
        f"--repo_env=CC={clang_path}",
        f"--repo_env=CXX={clang_path}++",
        f"--repo_env=LD={clang_path}",
    ]

    RunCommand(args, env=env, cwd=CRUBIT_SRC_DIR)


def ShutdownBazel():
    # This needs to use the same arguments as BuildCrubit, because otherwise
    # we get: WARNING: Running Bazel server needs to be killed, because the
    # startup options are different.
    RunCommand(["bazel", "shutdown"], cwd=CRUBIT_SRC_DIR)


def main():
    parser = argparse.ArgumentParser(
        description='Build and package Crubit tools')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        help='run subcommands with verbosity')
    parser.add_argument(
        '--skip-checkout',
        action='store_true',
        help='skip Crubit git checkout. Useful for trying local changes')
    args, rest = parser.parse_known_args()

    if not args.skip_checkout:
        CheckoutCrubit(CRUBIT_REVISION, CRUBIT_SRC_DIR)

    try:
        BuildCrubit()
    finally:
        ShutdownBazel()

    return 0


if __name__ == '__main__':
    sys.exit(main())
