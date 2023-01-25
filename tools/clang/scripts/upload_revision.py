#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script takes a Clang git revision as an argument, it then
creates a feature branch, puts this revision into update.py, uploads
a CL, triggers Clang Upload try bots, and tells what to do next"""

from __future__ import print_function

import argparse
import fnmatch
import itertools
import os
import re
import shutil
import subprocess
import sys
import urllib3

from build import CheckoutLLVM, GetCommitDescription, LLVM_DIR, RunCommand
from update import CHROMIUM_DIR, DownloadAndUnpack

# Path constants.
THIS_DIR = os.path.dirname(__file__)
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
CLANG_UPDATE_PY_PATH = os.path.join(THIS_DIR, 'update.py')
RUST_UPDATE_PY_PATH = os.path.join(THIS_DIR, '..', '..', 'rust',
                                   'update_rust.py')
BUILD_RUST_PY_PATH = os.path.join(THIS_DIR, '..', '..', 'rust', 'build_rust.py')
DEPS_PATH = os.path.join(CHROMIUM_DIR, 'DEPS')

# URL prefix for LLVM git diff ranges.
GOB_LLVM_URL = 'https://chromium.googlesource.com/external/github.com/llvm/llvm-project'

# Constants for finding HEAD.
CLANG_URL = 'https://api.github.com/repos/llvm/llvm-project/git/refs/heads/main'
CLANG_REGEX = b'"sha":"([^"]+)"'

# Constants for finding and downloading the latest Rust sources.
RUST_SRC_PATH = 'third_party/rust_src/src'
RUST_CIPD_PATH = 'chromium/third_party/rust_src'
# This is the URL where we can download packages found in CIPD.
# The complete format is:
# https://chrome-infra-packages.appspot.com/dl/path../+/version
RUST_CIPD_DOWNLOAD_URL = f'https://chrome-infra-packages.appspot.com/dl'
RUST_INSTANCES_REGEX = bytes('([0-9A-Za-z-_]+) │ .*? latest\W*\n', 'utf-8')
RUST_CIPD_VERSION_REGEX = '([0-9]+)@([0-9]{4})-([0-9]{2})-([0-9]{2})'
RUST_CIPD_DESCRIBE_REGEX = f'version:({RUST_CIPD_VERSION_REGEX})'.encode(
    'utf-8')

# Constant for updating the DEPS file for fetching the right Rust sources.
DEPS_RUST_SRC_SEARCH_REGEX = (
    '\'rust_toolchain_version\': \'version:(([0-9]+)@(.+?))\'')
DEPS_RUST_SRC_REPLACE_STRING = (
    '\'rust_toolchain_version\': \'version:{VERSION}\'')

# Bots where we build Clang + Rust.
BOTS = [
    'linux_upload_clang',
    'mac_upload_clang',
    'mac_upload_clang_arm',
    'win_upload_clang',
]

# Keep lines in here at <= 72 columns, else they wrap in gerrit.
COMMIT_FOOTER = \
'''
Bug: TODO. Remove the Tricium: line below when filling this in.
Tricium: skip
Cq-Include-Trybots: chromium/try:android-asan
Cq-Include-Trybots: chromium/try:chromeos-amd64-generic-cfi-thin-lto-rel
Cq-Include-Trybots: chromium/try:dawn-win10-x86-deps-rel
Cq-Include-Trybots: chromium/try:linux-chromeos-dbg
Cq-Include-Trybots: chromium/try:linux_chromium_cfi_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_chromeos_msan_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_msan_rel_ng
Cq-Include-Trybots: chromium/try:mac11-arm64-rel,mac_chromium_asan_rel_ng
Cq-Include-Trybots: chromium/try:ios-catalyst
Cq-Include-Trybots: chromium/try:win-asan
Cq-Include-Trybots: chromium/try:android-official,fuchsia-official
Cq-Include-Trybots: chromium/try:mac-official,linux-official
Cq-Include-Trybots: chromium/try:win-official,win32-official
Cq-Include-Trybots: chromium/try:linux-swangle-try-x64,win-swangle-try-x86
Cq-Include-Trybots: chrome/try:iphone-device,ipad-device
Cq-Include-Trybots: chrome/try:linux-chromeos-chrome
Cq-Include-Trybots: chrome/try:win-chrome,win64-chrome,linux-chrome,mac-chrome
Cq-Include-Trybots: chrome/try:linux-pgo,mac-pgo,win32-pgo,win64-pgo
Cq-Include-Trybots: luci.chromium.try:android-rust-arm-dbg
Cq-Include-Trybots: luci.chromium.try:android-rust-arm-rel
Cq-Include-Trybots: luci.chromium.try:linux-rust-x64-dbg
Cq-Include-Trybots: luci.chromium.try:linux-rust-x64-rel
'''

is_win = sys.platform.startswith('win32')


class RustVersion:
  """Holds the nightly Rust version in an explicit format."""

  def __init__(self, cipd_tag: int, year: int, month: int, day: int,
               sub_revision: int):
    self.cipd_tag = cipd_tag
    self.year = year
    self.month = month
    self.day = day
    self.sub_revision = sub_revision

  def from_cipd_date(cipd_date: str, sub_revision: SystemError):
    """The `cipd_date` has the format `TAG@YYYY-MM-DD`."""
    m = re.search(str(RUST_CIPD_VERSION_REGEX), cipd_date)
    assert len(m.groups()) == 4, "Rust date format expected TAG@YYYY-MM-DD"
    cipd_tag = int(m.group(1))
    year = int(m.group(2))
    month = int(m.group(3))
    day = int(m.group(4))
    return RustVersion(cipd_tag, year, month, day, int(sub_revision))

  def from_date_without_dashes(cipd_tag: str, date_without_dashes: str,
                               sub_revision: str):
    """The `date_with_dashes` has the format `YYYYMMDD`."""
    m = re.search('([0-9]{4})([0-9]{2})([0-9]{2})', date_without_dashes)
    assert len(m.groups()) == 3, "Rust date format expected YYYYMMDD"
    year = int(m.group(1))
    month = int(m.group(2))
    day = int(m.group(3))
    return RustVersion(int(cipd_tag), year, month, day, int(sub_revision))

  def string_with_dashes(self, with_tag: bool = False) -> str:
    s = ''
    if with_tag:
      s += f'{self.cipd_tag}@'
    s += f'{self.year:04}-{self.month:02}-{self.day:02}'
    return s

  def string_without_dashes(self,
                            with_tag: bool = False,
                            with_sub_revision: bool = False) -> str:
    s = ''
    if with_tag:
      s += f'{self.cipd_tag}@'
    s += f'{self.year:04}{self.month:02}{self.day:02}'
    if with_sub_revision:
      s += f'-{self.sub_revision}'
    return s

  def __str__(self) -> str:
    """A string containing the Rust version and sub revision.

    The string is useful for humans, it contains all info needed to identify
    the Rust version being built. It is also unique to a given Rust version and
    subversion.
    """
    return self.string_without_dashes(with_tag=True, with_sub_revision=True)

  def __eq__(self, o) -> bool:
    return (self.cipd_tag == o.cipd_tag and self.year == o.year
            and self.month == o.month and self.day == o.day
            and self.sub_revision == o.sub_revision)


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


def GetLatestClangGitHash():
  http = urllib3.PoolManager(cert_reqs="CERT_REQUIRED")
  resp = http.request('GET', CLANG_URL)
  if resp.status != 200:
    raise RuntimeError(f'Unable to download {CLANG_URL}: status {resp.status}')
  m = re.search(CLANG_REGEX, resp.data)
  return m.group(1).decode('utf-8')


def GetLatestRustVersion(sub_revision: int):
  cmd = ['cipd', 'instances', RUST_CIPD_PATH]
  output = subprocess.check_output(cmd)
  m = re.search(RUST_INSTANCES_REGEX, output)
  instance = m.group(1).decode('utf-8')
  if not instance:
    raise RuntimeError(f'No "latest" found in `{" ".join(cmd)}`')

  cmd = ['cipd', 'describe', RUST_CIPD_PATH, '-version', instance]
  output = subprocess.check_output(cmd)
  m = re.search(RUST_CIPD_DESCRIBE_REGEX, output)
  return RustVersion.from_cipd_date(m.group(1).decode('utf-8'), sub_revision)


def FetchRust(rust_version):
  if os.path.exists(RUST_SRC_PATH):
    print(f'Removing {RUST_SRC_PATH}.')
    shutil.rmtree(RUST_SRC_PATH)

  print(f'Fetching Rust {rust_version} into {RUST_SRC_PATH}')
  version_str = rust_version.string_with_dashes(with_tag=True)
  DownloadAndUnpack(
      f'{RUST_CIPD_DOWNLOAD_URL}/{RUST_CIPD_PATH}/+/version:{version_str}',
      RUST_SRC_PATH,
      is_known_zip=True)


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

  REV = '\'([0-9]+)\''
  SUB_REV = '([0-9]+)'
  TAG = '\'([0-9]+)\''

  tag = re.search(f'RUST_REVISION_TAG = {TAG}', content).group(1)
  date = re.search(f'RUST_REVISION = {REV}', content).group(1)
  sub_revision = re.search(f'RUST_SUB_REVISION = {SUB_REV}', content).group(1)
  old_version = RustVersion.from_date_without_dashes(tag, date, sub_revision)

  content = re.sub(f'RUST_REVISION_TAG = {TAG}',
                   f'RUST_REVISION_TAG = \'{new_version.cipd_tag}\'',
                   content,
                   count=1)
  content = re.sub(f'RUST_REVISION = {REV}',
                   f'RUST_REVISION = \'{new_version.string_without_dashes()}\'',
                   content,
                   count=1)
  content = re.sub(f'RUST_SUB_REVISION = {SUB_REV}',
                   f'RUST_SUB_REVISION = {new_version.sub_revision}',
                   content,
                   count=1)

  with open(RUST_UPDATE_PY_PATH, 'w') as f:
    f.write(content)

  with open(DEPS_PATH, 'r') as f:
    deps = f.read()

  DATE = '([0-9-]+)'

  old_deps_match = re.search(DEPS_RUST_SRC_SEARCH_REGEX, deps)
  assert len(old_deps_match.groups()
             ) == 3, 'Unable to find `rust_toolchain_version` in //DEPS file.'
  assert old_deps_match.group(1) == old_version.string_with_dashes(
      with_tag=True), (
          f'The Rust version from {RUST_UPDATE_PY_PATH} does not match '
          '`rust_toolchain_version` in //DEPS file. They should always match.')

  deps = re.sub(DEPS_RUST_SRC_SEARCH_REGEX,
                DEPS_RUST_SRC_REPLACE_STRING.format(
                    VERSION=new_version.string_with_dashes(with_tag=True)),
                deps,
                count=1)

  with open(DEPS_PATH, 'w') as f:
    f.write(deps)

  return old_version


def PatchRustStage0():
  verify_stage0 = subprocess.run([BUILD_RUST_PY_PATH, '--verify-stage0-hash'],
                                 capture_output=True,
                                 text=True)
  if verify_stage0.returncode == 0:
    return

  # TODO(crbug.com/1405814): We're printing a warning that the hash has
  # changed, but we could require a verification step of some sort here. We
  # should do the same for both Rust and Clang if we do so.
  print(verify_stage0.stdout)
  lines = verify_stage0.stdout.splitlines()
  m = re.match('Actual hash: +([0-9a-z]+)', lines[2])
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


def PatchRustRemoveFallback():
  with open(RUST_UPDATE_PY_PATH) as f:
    content = f.read()

  CLANG_HASH = '([0-9a-z-]+)'
  content = re.sub(f'FALLBACK_CLANG_VERSION = \'{CLANG_HASH}\'',
                   f'FALLBACK_CLANG_VERSION = \'\'',
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
  # TODO(crbug.com/1401042): Remove this when the cron job doesn't pass a SHA.
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
  parser.add_argument(
      '--rust-cipd-version',
      type=str,
      metavar='TAG@YYYY-MM-DD',
      help=
      ('Rust version to build the toolchain for. The version string comes from '
       '`cipd describe chromium/third_party/rust_src -version <INSTANCE>`, '
       'where the <INSTANCE> comes from '
       '`cipd instances chromium/third_party/rust_src`'))
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

  args = parser.parse_args()

  if args.clang_git_hash:
    clang_git_hash = args.clang_git_hash
  else:
    clang_git_hash = GetLatestClangGitHash()

  # To `GetCommitDescription()`, we need a checkout. On success, the
  # CheckoutLLVM() makes `LLVM_DIR` be the current working directory, so that
  # we can GetCommitDescription() without changing directory.
  CheckoutLLVM(clang_git_hash, LLVM_DIR)
  clang_version = ClangVersion(GetCommitDescription(clang_git_hash),
                               args.clang_sub_revision)
  os.chdir(CHROMIUM_DIR)

  if args.rust_cipd_version:
    rust_version = RustVersion.from_cipd_date(args.rust_cipd_version,
                                              args.rust_sub_revision)
  else:
    rust_version = GetLatestRustVersion(args.rust_sub_revision)

  FetchRust(rust_version)

  print((f'Making a patch for Clang {clang_version} and Rust {rust_version}'))

  branch_name = f'clang-{clang_version}_rust-{rust_version}'
  Git('checkout', 'origin/main', '-b', branch_name, no_run=args.no_git)

  old_clang_version = PatchClangRevision(clang_version)
  # Avoiding changing Rust versions when rolling Clang until we can fetch
  # stdlib sources at the same revisionas the compiler, from the
  # FALLBACK_REVISION in update.py.
  roll_rust = False
  if roll_rust:
    old_rust_version = PatchRustRevision(rust_version)
    assert (clang_version != old_clang_version
            or rust_version != old_rust_version), (
                'Change the sub-revision of Clang or Rust if there is '
                'no major version change.')
    # TODO: Turn the nightly dates into git hashes?
    PatchRustStage0()
    # TODO: Do this when we block Clang updates without a matching Rust
    # compiler.
    # PatchRustRemoveFallback()
  else:
    assert (clang_version !=
            old_clang_version), ('Change the sub-revision of Clang if there is '
                                 'no major version change.')

  clang_change = f'{old_clang_version} : {clang_version}'
  clang_change_log = (
      f'{GOB_LLVM_URL}/+log/'
      f'{old_clang_version.short_git_hash}..{clang_version.short_git_hash}')

  if roll_rust:
    rust_change = (
        f'{old_rust_version.string_without_dashes(with_sub_revision = True)} '
        f': {rust_version.string_without_dashes(with_sub_revision = True)}')
  else:
    rust_change = '[skipping Rust]'

  title = f'Roll clang+rust {clang_change} / {rust_change}'

  cmd = ' '.join(sys.argv)
  body = f'{clang_change_log}\n\nRan: {cmd}'

  commit_message = f'{title}\n\n{body}\n{COMMIT_FOOTER}'

  Git('add',
      CLANG_UPDATE_PY_PATH,
      RUST_UPDATE_PY_PATH,
      DEPS_PATH,
      no_run=args.no_git)
  Git('commit', '-m', commit_message, no_run=args.no_git)
  Git('cl', 'upload', '-f', '--bypass-hooks', no_run=args.no_git)
  Git('cl',
      'try',
      '-B',
      "chromium/try",
      *itertools.chain(*[['-b', bot] for bot in BOTS]),
      no_run=args.no_git)

  print('Please, wait until the try bots succeeded '
        'and then push the binaries to goma.')


if __name__ == '__main__':
  sys.exit(main())
