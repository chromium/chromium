#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to download prebuilt clang binaries. It runs as a
"gclient hook" in Chromium checkouts.

It can also be run stand-alone as a convenient way of installing a well-tested
near-tip-of-tree clang version:

  $ curl -s https://raw.githubusercontent.com/chromium/chromium/master/tools/clang/scripts/update.py | python - --output-dir=/tmp/clang

(Note that the output dir may be deleted and re-created if it exists.)
"""

from __future__ import division
from __future__ import print_function
import argparse
import os
import shutil
import stat
import sys
import tarfile
import tempfile
import time

try:
  from urllib2 import HTTPError, URLError, urlopen
except ImportError: # For Py3 compatibility
  from urllib.error import HTTPError, URLError
  from urllib.request import urlopen

import zipfile


# Do NOT CHANGE this if you don't know what you're doing -- see
# https://chromium.googlesource.com/chromium/src/+/master/docs/updating_clang.md
# Reverting problematic clang rolls is safe, though.
# This is the output of `git describe` and is usable as a commit-ish.
CLANG_REVISION = 'llvmorg-12-init-5035-gd0abc757'
CLANG_SUB_REVISION = 3

PACKAGE_VERSION = '%s-%s' % (CLANG_REVISION, CLANG_SUB_REVISION)
RELEASE_VERSION = '12.0.0'


CDS_URL = os.environ.get('CDS_CLANG_BUCKET_OVERRIDE',
    'https://commondatastorage.googleapis.com/chromium-browser-clang')

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts')

STAMP_FILE = os.path.normpath(
    os.path.join(LLVM_BUILD_DIR, 'cr_build_revision'))
OLD_STAMP_FILE = os.path.normpath(
    os.path.join(LLVM_BUILD_DIR, '..', 'cr_build_revision'))
FORCE_HEAD_REVISION_FILE = os.path.normpath(os.path.join(LLVM_BUILD_DIR, '..',
                                                   'force_head_revision'))


def RmTree(dir):
  """Delete dir."""
  def ChmodAndRetry(func, path, _):
    # Subversion can leave read-only files around.
    if not os.access(path, os.W_OK):
      os.chmod(path, stat.S_IWUSR)
      return func(path)
    raise
  shutil.rmtree(dir, onerror=ChmodAndRetry)


def ReadStampFile(path):
  """Return the contents of the stamp file, or '' if it doesn't exist."""
  try:
    with open(path, 'r') as f:
      return f.read().rstrip()
  except IOError:
    return ''


def WriteStampFile(s, path):
  """Write s to the stamp file."""
  EnsureDirExists(os.path.dirname(path))
  with open(path, 'w') as f:
    f.write(s)
    f.write('\n')


def DownloadUrl(url, output_file):
  """Download url into output_file."""
  CHUNK_SIZE = 4096
  TOTAL_DOTS = 10
  num_retries = 3
  retry_wait_s = 5  # Doubled at each retry.

  while True:
    try:
      sys.stdout.write('Downloading %s ' % url)
      sys.stdout.flush()
      response = urlopen(url)
      total_size = int(response.info().get('Content-Length').strip())
      bytes_done = 0
      dots_printed = 0
      while True:
        chunk = response.read(CHUNK_SIZE)
        if not chunk:
          break
        output_file.write(chunk)
        bytes_done += len(chunk)
        num_dots = TOTAL_DOTS * bytes_done // total_size
        sys.stdout.write('.' * (num_dots - dots_printed))
        sys.stdout.flush()
        dots_printed = num_dots
      if bytes_done != total_size:
        raise URLError("only got %d of %d bytes" %
                       (bytes_done, total_size))
      print(' Done.')
      return
    except URLError as e:
      sys.stdout.write('\n')
      print(e)
      if num_retries == 0 or isinstance(e, HTTPError) and e.code == 404:
        raise e
      num_retries -= 1
      print('Retrying in %d s ...' % retry_wait_s)
      sys.stdout.flush()
      time.sleep(retry_wait_s)
      retry_wait_s *= 2


def EnsureDirExists(path):
  if not os.path.exists(path):
    os.makedirs(path)


def DownloadAndUnpack(url, output_dir, path_prefixes=None):
  """Download an archive from url and extract into output_dir. If path_prefixes
     is not None, only extract files whose paths within the archive start with
     any prefix in path_prefixes."""
  with tempfile.TemporaryFile() as f:
    DownloadUrl(url, f)
    f.seek(0)
    EnsureDirExists(output_dir)
    if url.endswith('.zip'):
      assert path_prefixes is None
      zipfile.ZipFile(f).extractall(path=output_dir)
    else:
      t = tarfile.open(mode='r:gz', fileobj=f)
      members = None
      if path_prefixes is not None:
        members = [m for m in t.getmembers()
                   if any(m.name.startswith(p) for p in path_prefixes)]
      t.extractall(path=output_dir, members=members)


def GetPlatformUrlPrefix(host_os):
  _HOST_OS_URL_MAP = {
      'linux': 'Linux_x64',
      'mac': 'Mac',
      'win': 'Win',
  }
  return CDS_URL + '/' + _HOST_OS_URL_MAP[host_os] + '/'


def DownloadAndUnpackPackage(package_file, output_dir, host_os):
  cds_file = "%s-%s.tgz" % (package_file, PACKAGE_VERSION)
  cds_full_url = GetPlatformUrlPrefix(host_os) + cds_file
  try:
    DownloadAndUnpack(cds_full_url, output_dir)
  except URLError:
    print('Failed to download prebuilt clang package %s' % cds_file)
    print('Use build.py if you want to build locally.')
    print('Exiting.')
    sys.exit(1)


# TODO(hans): Create a clang-win-runtime package instead.
def DownloadAndUnpackClangWinRuntime(output_dir):
  cds_file = "clang-%s.tgz" %  PACKAGE_VERSION
  cds_full_url = GetPlatformUrlPrefix('win') + cds_file
  path_prefixes =  [ 'lib/clang/' + RELEASE_VERSION + '/lib/',
                     'bin/llvm-symbolizer.exe' ]
  try:
    DownloadAndUnpack(cds_full_url, output_dir, path_prefixes)
  except URLError:
    print('Failed to download prebuilt clang %s' % cds_file)
    print('Use build.py if you want to build locally.')
    print('Exiting.')
    sys.exit(1)


def UpdatePackage(package_name, host_os):
  stamp_file = None
  package_file = None

  stamp_file = os.path.join(LLVM_BUILD_DIR, package_name + '_revision')
  if package_name == 'clang':
    stamp_file = STAMP_FILE
    package_file = 'clang'
  elif package_name == 'clang-tidy':
    package_file = 'clang-tidy'
  elif package_name == 'lld_mac':
    package_file = 'lld'
    if host_os != 'mac':
      print(
          'The lld_mac package cannot be downloaded for non-macs.',
          file=sys.stderr)
      print(
          'On non-mac, lld is included in the clang package.', file=sys.stderr)
      return 1
  elif package_name == 'objdump':
    package_file = 'llvmobjdump'
  elif package_name == 'translation_unit':
    package_file = 'translation_unit'
  elif package_name == 'coverage_tools':
    stamp_file = os.path.join(LLVM_BUILD_DIR, 'cr_coverage_revision')
    package_file = 'llvm-code-coverage'
  elif package_name == 'libclang':
    package_file = 'libclang'
  else:
    print('Unknown package: "%s".' % package_name)
    return 1

  assert stamp_file is not None
  assert package_file is not None

  # TODO(hans): Create a clang-win-runtime package and use separate DEPS hook.
  target_os = []
  if package_name == 'clang':
    try:
      GCLIENT_CONFIG = os.path.join(os.path.dirname(CHROMIUM_DIR), '.gclient')
      env = {}
      exec (open(GCLIENT_CONFIG).read(), env, env)
      target_os = env.get('target_os', target_os)
    except:
      pass

  if os.path.exists(OLD_STAMP_FILE):
    # Delete the old stamp file so it doesn't look like an old version of clang
    # is available in case the user rolls back to an old version of this script
    # during a bisect for example (crbug.com/988933).
    os.remove(OLD_STAMP_FILE)

  expected_stamp = ','.join([PACKAGE_VERSION] + target_os)
  if ReadStampFile(stamp_file) == expected_stamp:
    return 0

  # Updating the main clang package nukes the output dir. Any other packages
  # need to be updated *after* the clang package.
  if package_name == 'clang' and os.path.exists(LLVM_BUILD_DIR):
    RmTree(LLVM_BUILD_DIR)

  DownloadAndUnpackPackage(package_file, LLVM_BUILD_DIR, host_os)

  if package_name == 'clang' and 'win' in target_os:
    # When doing win/cross builds on other hosts, get the Windows runtime
    # libraries, and llvm-symbolizer.exe (needed in asan builds).
    DownloadAndUnpackClangWinRuntime(LLVM_BUILD_DIR)

  WriteStampFile(expected_stamp, stamp_file)
  return 0


def main():
  _PLATFORM_HOST_OS_MAP = {
      'darwin': 'mac',
      'cygwin': 'win',
      'linux2': 'linux',
      'win32': 'win',
  }
  default_host_os = _PLATFORM_HOST_OS_MAP.get(sys.platform, sys.platform)

  parser = argparse.ArgumentParser(description='Update clang.')
  parser.add_argument('--output-dir',
                      help='Where to extract the package.')
  parser.add_argument('--package',
                      help='What package to update (default: clang)',
                      default='clang')
  parser.add_argument(
      '--host-os',
      help='Which host OS to download for (default: %s)' % default_host_os,
      default=default_host_os,
      choices=('linux', 'mac', 'win'))
  parser.add_argument('--force-local-build', action='store_true',
                      help='(no longer used)')
  parser.add_argument('--print-revision', action='store_true',
                      help='Print current clang revision and exit.')
  parser.add_argument('--llvm-force-head-revision', action='store_true',
                      help='Print locally built revision with --print-revision')
  parser.add_argument('--print-clang-version', action='store_true',
                      help=('Print current clang release version (e.g. 9.0.0) '
                            'and exit.'))
  parser.add_argument('--verify-version',
                      help='Verify that clang has the passed-in version.')
  args = parser.parse_args()

  if args.force_local_build:
    print(('update.py --force-local-build is no longer used to build clang; '
           'use build.py instead.'))
    return 1

  if args.verify_version and args.verify_version != RELEASE_VERSION:
    print('RELEASE_VERSION is %s but --verify-version argument was %s.' % (
        RELEASE_VERSION, args.verify_version))
    print('clang_version in build/toolchain/toolchain.gni is likely outdated.')
    return 1

  if args.print_clang_version:
    print(RELEASE_VERSION)
    return 0

  if args.print_revision:
    if args.llvm_force_head_revision:
      force_head_revision = ReadStampFile(FORCE_HEAD_REVISION_FILE)
      if force_head_revision == '':
        print('No locally built version found!')
        return 1
      print(force_head_revision)
      return 0

    print(PACKAGE_VERSION)
    return 0

  if args.llvm_force_head_revision:
    print('--llvm-force-head-revision can only be used for --print-revision')
    return 1

  if args.output_dir:
    global LLVM_BUILD_DIR, STAMP_FILE
    LLVM_BUILD_DIR = os.path.abspath(args.output_dir)
    STAMP_FILE = os.path.join(LLVM_BUILD_DIR, 'cr_build_revision')

  return UpdatePackage(args.package, args.host_os)


if __name__ == '__main__':
  sys.exit(main())
