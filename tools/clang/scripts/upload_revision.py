#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script takes a Clang git revision as an argument, it then
creates a feature branch, puts this revision into update.py, uploads
a CL, triggers Clang Upload try bots, and tells what to do next"""

from __future__ import print_function

import argparse
import itertools
import os
import re
import subprocess
import sys
import urllib.request

from build import (CheckoutGitRepo, GetCommitDescription, GetLatestLLVMCommit,
                   LLVM_DIR, LLVM_GIT_URL, RunCommand)
from update import CHROMIUM_DIR, DownloadAndUnpack

# Access to //tools/rust
sys.path.append(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..',
                 'rust'))

from build_rust import RUST_GIT_URL, RUST_SRC_DIR, GetLatestRustCommit

# Path constants.
THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
CLANG_UPDATE_PY_PATH = os.path.join(THIS_DIR, 'update.py')
RUST_UPDATE_PY_PATH = os.path.join(THIS_DIR, '..', '..', 'rust',
                                   'update_rust.py')
BUILD_RUST_PY_PATH = os.path.join(THIS_DIR, '..', '..', 'rust', 'build_rust.py')

# Bots where we build Clang + Rust.
BUILD_CLANG_BOTS = [
    'linux_upload_clang',
    'mac_upload_clang',
    'mac_upload_clang_arm',
    'win_upload_clang',
]
BUILD_RUST_BOTS = [
    'linux_upload_rust',
    'mac_upload_rust',
    'mac_upload_rust_arm',
    'win_upload_rust',
]

# Keep lines in here at <= 72 columns, else they wrap in gerrit.
# There can be no whitespace line between or below these gerrit footers.
COMMIT_FOOTER = \
'''
Bug: TODO. Remove the Tricium: line below when filling this in.
Tricium: skip
Disable-Rts: True
Cq-Include-Trybots: chromium/try:chromeos-amd64-generic-cfi-thin-lto-rel
Cq-Include-Trybots: chromium/try:dawn-win10-x86-deps-rel
Cq-Include-Trybots: chromium/try:linux-chromeos-dbg
Cq-Include-Trybots: chromium/try:linux_chromium_cfi_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_chromeos_msan_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_msan_rel_ng
Cq-Include-Trybots: chromium/try:mac11-arm64-rel,mac_chromium_asan_rel_ng
Cq-Include-Trybots: chromium/try:ios-catalyst,win-asan,android-official
Cq-Include-Trybots: chromium/try:fuchsia-arm64-cast-receiver-rel
Cq-Include-Trybots: chromium/try:mac-official,linux-official
Cq-Include-Trybots: chromium/try:win-official,win32-official
Cq-Include-Trybots: chromium/try:win-arm64-rel
Cq-Include-Trybots: chromium/try:linux-swangle-try-x64,win-swangle-try-x86
Cq-Include-Trybots: chromium/try:android-cronet-mainline-clang-arm64-dbg
Cq-Include-Trybots: chromium/try:android-cronet-mainline-clang-arm64-rel
Cq-Include-Trybots: chromium/try:android-cronet-mainline-clang-riscv64-dbg
Cq-Include-Trybots: chromium/try:android-cronet-mainline-clang-riscv64-rel
Cq-Include-Trybots: chromium/try:android-cronet-mainline-clang-x86-dbg
Cq-Include-Trybots: chromium/try:android-cronet-mainline-clang-x86-rel
Cq-Include-Trybots: chromium/try:android-cronet-riscv64-dbg
Cq-Include-Trybots: chromium/try:android-cronet-riscv64-rel
Cq-Include-Trybots: chrome/try:iphone-device,ipad-device
Cq-Include-Trybots: chrome/try:linux-chromeos-chrome
Cq-Include-Trybots: chrome/try:win-chrome,win64-chrome,linux-chrome,mac-chrome
Cq-Include-Trybots: chrome/try:linux-pgo,mac-pgo,win32-pgo,win64-pgo'''

RUST_BOTS = \
'''Cq-Include-Trybots: chromium/try:android-rust-arm32-rel
Cq-Include-Trybots: chromium/try:android-rust-arm64-dbg
Cq-Include-Trybots: chromium/try:android-rust-arm64-rel
Cq-Include-Trybots: chromium/try:linux-rust-x64-dbg
Cq-Include-Trybots: chromium/try:linux-rust-x64-rel
Cq-Include-Trybots: chromium/try:mac-rust-x64-dbg
Cq-Include-Trybots: chromium/try:win-rust-x64-dbg
Cq-Include-Trybots: chromium/try:win-rust-x64-rel'''

is_win = sys.platform.startswith('win32')


class RustVersion:
  """Holds the nightly Rust version in an explicit format."""

  def __init__(self, git_hash: str, sub_revision: int):
    self.git_hash = git_hash
    self.short_git_hash = git_hash[slice(0, 12)]
    self.sub_revision = sub_revision

  def __str__(self) -> str:
    """A string containing the Rust version and sub revision.

    The string is useful for humans, it contains all info needed to identify
    the Rust version being built. It is also unique to a given Rust version and
    subversion.
    """
    return f'{self.git_hash}-{self.sub_revision}'

  def __eq__(self, o) -> bool:
    return (self.git_hash == o.git_hash and self.sub_revision == o.sub_revision)


class ClangVersion:
  """Holds the Clang version in an explicit format."""

  def __init__(self, git_describe: str, sub_revision: str):
    self.git_describe = git_describe
    self.short_git_hash = re.search('-g([0-9a-f]+)', git_describe).group(1)
    self.sub_revision = int(sub_revision)

  def __str__(self) -> str:
    """A string containing the Clang version and sub revision.

    The string is useful for humans, it contains all info needed to identify
    the Clang version being built. It is also unique to a given Clang version
    and subversion.
    """
    return f'{self.git_describe}-{self.sub_revision}'

  def __eq__(self, o) -> bool:
    return (self.git_describe == o.git_describe
            and self.sub_revision == o.sub_revision)


def PatchClangRevision(new_version: ClangVersion) -> ClangVersion:
  with open(CLANG_UPDATE_PY_PATH) as f:
    content = f.read()

  REV = '\'([0-9a-z-]+)\''
  SUB_REV = '([0-9]+)'

  git_describe = re.search(f'CLANG_REVISION = {REV}', content).group(1)
  sub_revision = re.search(f'CLANG_SUB_REVISION = {SUB_REV}', content).group(1)
  old_version = ClangVersion(git_describe, sub_revision)

  content = re.sub(f'CLANG_REVISION = {REV}',
                   f'CLANG_REVISION = \'{new_version.git_describe}\'',
                   content,
                   count=1)
  content = re.sub(f'CLANG_SUB_REVISION = {SUB_REV}',
                   f'CLANG_SUB_REVISION = {new_version.sub_revision}',
                   content,
                   count=1)

  with open(CLANG_UPDATE_PY_PATH, 'w') as f:
    f.write(content)

  return old_version


def PatchRustRevision(new_version: RustVersion) -> RustVersion:
  with open(RUST_UPDATE_PY_PATH) as f:
    content = f.read()

  REV = '\'([0-9a-z-]+)\''
  SUB_REV = '([0-9]+)'

  git_hash = re.search(f'RUST_REVISION = {REV}', content).group(1)
  sub_revision = re.search(f'RUST_SUB_REVISION = {SUB_REV}', content).group(1)
  old_version = RustVersion(git_hash, sub_revision)

  content = re.sub(f'RUST_REVISION = {REV}',
                   f'RUST_REVISION = \'{new_version.git_hash}\'',
                   content,
                   count=1)
  content = re.sub(f'RUST_SUB_REVISION = {SUB_REV}',
                   f'RUST_SUB_REVISION = {new_version.sub_revision}',
                   content,
                   count=1)

  with open(RUST_UPDATE_PY_PATH, 'w') as f:
    f.write(content)

  return old_version


def PatchRustStage0():
  verify_stage0 = subprocess.run(
      [sys.executable, BUILD_RUST_PY_PATH, '--verify-stage0-hash'],
      capture_output=True,
      text=True)
  if verify_stage0.returncode == 0:
    return

  # TODO(crbug.com/40252478): We're printing a warning that the hash has
  # changed, but we could require a verification step of some sort here. We
  # should do the same for both Rust and Clang if we do so.
  print(verify_stage0.stdout)
  lines = verify_stage0.stdout.splitlines()
  m = re.match('Actual hash: +([0-9a-z]+)', lines[-1])
  new_stage0_hash = m.group(1)

  with open(RUST_UPDATE_PY_PATH) as f:
    content = f.read()

  STAGE0_HASH = '\'([0-9a-z]+)\''
  content = re.sub(f'STAGE0_JSON_SHA256 = {STAGE0_HASH}',
                   f'STAGE0_JSON_SHA256 = \'{new_stage0_hash}\'',
                   content,
                   count=1)
  with open(RUST_UPDATE_PY_PATH, 'w') as f:
    f.write(content)


def PatchRustRemoveOverride():
  with open(RUST_UPDATE_PY_PATH) as f:
    content = f.read()

  REV = '([0-9a-z-]+)'
  content = re.sub(f'OVERRIDE_CLANG_REVISION = \'{REV}\'',
                   f'OVERRIDE_CLANG_REVISION = None',
                   content,
                   count=1)
  with open(RUST_UPDATE_PY_PATH, 'w') as f:
    f.write(content)


def Git(*args, no_run: bool):
  """Runs a git command, or just prints it out if `no_run` is True."""
  if no_run:
    print('\033[91m', end='')  # Color red
    print('Skipped running: ', end='')
    print('\033[0m', end='')  # No color
    print(*['git'] + [f'\'{i}\'' for i in list(args)], end='')
    print()
  else:
    # Needs shell=True on Windows due to git.bat in depot_tools.
    subprocess.check_call(['git'] + list(args), shell=is_win)


def main():
  parser = argparse.ArgumentParser(description='upload new clang revision')
  # TODO(crbug.com/40250560): Remove this when the cron job doesn't pass a SHA.
  parser.add_argument(
      'ignored',
      nargs='?',
      help='Ignored argument to handle the cron job passing a clang SHA')
  parser.add_argument('--clang-git-hash',
                      type=str,
                      metavar='SHA1',
                      help='Clang git hash to build the toolchain for.')
  parser.add_argument(
      '--clang-sub-revision',
      type=int,
      default=1,
      metavar='NUM',
      help='Clang sub-revision to build the toolchain for. Defaults to 1.')
  parser.add_argument('--rust-git-hash',
                      type=str,
                      metavar='SHA1',
                      help='Rust git hash to build the toolchain for.')
  parser.add_argument(
      '--rust-sub-revision',
      type=int,
      default=1,
      metavar='NUM',
      help='Rust sub-revision to build the toolchain for. Defaults to 1.')
  parser.add_argument(
      '--no-git',
      action='store_true',
      default=False,
      help=('Print out `git` commands instead of running them. Still generates '
            'a local diff for debugging purposes.'))
  parser.add_argument('--skip-rust',
                      action='store_true',
                      default=False,
                      help=('Skip updating the rust revision.'))
  parser.add_argument('--skip-clang',
                      action='store_true',
                      default=False,
                      help=('Skip updating the clang revision.'))

  args = parser.parse_args()

  if args.skip_clang and args.skip_rust:
    print('Cannot set both --skip-clang and --skip-rust.')
    sys.exit(1)

  if args.skip_clang:
    clang_version = '-skipped-'
  else:
    if args.clang_git_hash:
      clang_git_hash = args.clang_git_hash
    else:
      clang_git_hash = GetLatestLLVMCommit()
    # To `GetCommitDescription()`, we need a checkout. On success, the
    # CheckoutLLVM() makes `LLVM_DIR` be the current working directory, so that
    # we can GetCommitDescription() without changing directory.
    CheckoutGitRepo("LLVM", LLVM_GIT_URL, clang_git_hash, LLVM_DIR)
    clang_version = ClangVersion(GetCommitDescription(clang_git_hash),
                                 args.clang_sub_revision)
    os.chdir(CHROMIUM_DIR)

  if args.skip_rust:
    rust_version = '-skipped-'
  else:
    if args.rust_git_hash:
      rust_git_hash = args.rust_git_hash
    else:
      rust_git_hash = GetLatestRustCommit()
    CheckoutGitRepo("Rust", RUST_GIT_URL, rust_git_hash, RUST_SRC_DIR)
    rust_version = RustVersion(rust_git_hash, args.rust_sub_revision)
    os.chdir(CHROMIUM_DIR)

  print(f'Making a patch for Clang {clang_version} and Rust {rust_version}')

  branch_name = f'clang-{clang_version}_rust-{rust_version}'
  Git('checkout', 'origin/main', '-b', branch_name, no_run=args.no_git)

  old_clang_version = clang_version
  if not args.skip_clang:
    old_clang_version = PatchClangRevision(clang_version)
  if args.skip_rust:
    assert (clang_version !=
            old_clang_version), ('Change the sub-revision of Clang if there is '
                                 'no major version change.')
  else:
    old_rust_version = PatchRustRevision(rust_version)
    assert (clang_version != old_clang_version
            or rust_version != old_rust_version), (
                'Change the sub-revision of Clang or Rust if there is '
                'no major version change.')
    PatchRustStage0()
    if not args.skip_clang:
      PatchRustRemoveOverride()

  if args.skip_clang:
    clang_change = '[skipping Clang]'
    clang_change_log = ''
  else:
    clang_change = f'{old_clang_version} : {clang_version}'
    clang_change_log = (
        f'{LLVM_GIT_URL}/+log/'
        f'{old_clang_version.short_git_hash}..{clang_version.short_git_hash}'
        f'\n\n')

  if args.skip_rust:
    rust_change = '[skipping Rust]'
    rust_change_log = ''
  else:
    rust_change = f'{old_rust_version} : {rust_version}'
    rust_change_log = (f'{RUST_GIT_URL}/+log/'
                       f'{old_rust_version.short_git_hash}..'
                       f'{rust_version.short_git_hash}'
                       f'\n\n')

  title = f'Roll clang+rust {clang_change} / {rust_change}'

  cmd = ' '.join(sys.argv)
  body = f'{clang_change_log}{rust_change_log}Ran: {cmd}'

  commit_message = f'{title}\n\n{body}\n{COMMIT_FOOTER}'
  if not args.skip_rust:
    commit_message += f'\n{RUST_BOTS}'

  Git('add',
      CLANG_UPDATE_PY_PATH,
      RUST_UPDATE_PY_PATH,
      no_run=args.no_git)
  Git('commit', '-m', commit_message, no_run=args.no_git)
  Git('cl', 'upload', '-f', '--bypass-hooks', '--squash', no_run=args.no_git)
  if not args.skip_clang:
    Git('cl',
        'try',
        '-B',
        "chromium/try",
        *itertools.chain(*[['-b', bot] for bot in BUILD_CLANG_BOTS]),
        no_run=args.no_git)

  Git('cl',
      'try',
      '-B',
      "chromium/try",
      *itertools.chain(*[['-b', bot] for bot in BUILD_RUST_BOTS]),
      no_run=args.no_git)

  print('Please, wait until the try bots succeeded '
        'and then push the binaries to RBE.')
  print()
  print('To update the Clang/Rust DEPS entries, run:\n  '
        'tools/clang/scripts/sync_deps.py')
  print()
  print('To regenerate BUILD.gn rules for Rust stdlib (needed if dep versions '
        'in the stdlib change for example), run:\n  tools/rust/gnrt_stdlib.py.')
  print()
  print('To update Abseil .def files, run:\n  '
        'third_party/abseil-cpp/generate_def_files.py')


if __name__ == '__main__':
  sys.exit(main())
