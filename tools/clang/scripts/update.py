#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to download prebuilt clang binaries. It runs as a
"gclient hook" in Chromium checkouts.

It can also be run stand-alone as a convenient way of installing a well-tested
near-tip-of-tree clang version:

  $ curl -s https://raw.githubusercontent.com/chromium/chromium/main/tools/clang/scripts/update.py | python3 - --output-dir=/tmp/clang

(Note that the output dir may be deleted and re-created if it exists.)
"""

import sys
assert sys.version_info >= (3, 0), 'This script requires Python 3.'

import argparse
import glob
import os
import platform
import shutil
import stat
import tarfile
import tempfile
import time
import urllib.request
import urllib.error
import zipfile
import zlib


# Do NOT CHANGE this if you don't know what you're doing -- see
# https://chromium.googlesource.com/chromium/src/+/main/docs/updating_clang.md
# Reverting problematic clang rolls is safe, though.
# This is the output of `git describe` and is usable as a commit-ish.
CLANG_REVISION = 'llvmorg-20-init-3847-g69c43468'
CLANG_SUB_REVISION = 28

PACKAGE_VERSION = '%s-%s' % (CLANG_REVISION, CLANG_SUB_REVISION)
RELEASE_VERSION = '20'

CDS_URL = os.environ.get('CDS_CLANG_BUCKET_OVERRIDE',
    'https://commondatastorage.googleapis.com/chromium-browser-clang')

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts')

STAMP_FILENAME = 'cr_build_revision'
STAMP_FILE = os.path.normpath(os.path.join(LLVM_BUILD_DIR, STAMP_FILENAME))
OLD_STAMP_FILE = os.path.normpath(
    os.path.join(LLVM_BUILD_DIR, '..', STAMP_FILENAME))
FORCE_HEAD_REVISION_FILE = os.path.normpath(
    os.path.join(LLVM_BUILD_DIR, '..', 'force_head_revision'))


def RmTree(dir):
  """Delete dir."""
  if sys.platform == 'win32':
    # Avoid problems with paths longer than MAX_PATH
    # https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    dir = f'\\\\?\\{dir}'

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
      sys.stdout.write(f'Downloading {url} ')
      sys.stdout.flush()
      request = urllib.request.Request(url)
      request.add_header('Accept-Encoding', 'gzip')
      response = urllib.request.urlopen(request)
      total_size = None
      if 'Content-Length' in response.headers:
        total_size = int(response.headers['Content-Length'].strip())

      is_gzipped = response.headers.get('Content-Encoding',
                                        '').strip() == 'gzip'
      if is_gzipped:
        gzip_decode = zlib.decompressobj(zlib.MAX_WBITS + 16)

      bytes_done = 0
      dots_printed = 0
      while True:
        chunk = response.read(CHUNK_SIZE)
        if not chunk:
          break
        bytes_done += len(chunk)

        if is_gzipped:
          chunk = gzip_decode.decompress(chunk)
        output_file.write(chunk)

        if total_size is not None:
          num_dots = TOTAL_DOTS * bytes_done // total_size
          sys.stdout.write('.' * (num_dots - dots_printed))
          sys.stdout.flush()
          dots_printed = num_dots
      if total_size is not None and bytes_done != total_size:
        raise urllib.error.URLError(
            f'only got {bytes_done} of {total_size} bytes')
      if is_gzipped:
        output_file.write(gzip_decode.flush())
      print(' Done.')
      return
    except (ConnectionError, urllib.error.URLError) as e:
      sys.stdout.write('\n')
      print(e)
      if num_retries == 0 or isinstance(
          e, urllib.error.HTTPError) and e.code == 404:
        raise e
      num_retries -= 1
      output_file.seek(0)
      output_file.truncate()
      print(f'Retrying in {retry_wait_s} s ...')
      sys.stdout.flush()
      time.sleep(retry_wait_s)
      retry_wait_s *= 2


def EnsureDirExists(path):
  if not os.path.exists(path):
    os.makedirs(path)


def DownloadAndUnpack(url, output_dir, path_prefixes=None, is_known_zip=False):
  """Download an archive from url and extract into output_dir. If path_prefixes
     is not None, only extract files whose paths within the archive start with
     any prefix in path_prefixes."""
  with tempfile.TemporaryFile() as f:
    DownloadUrl(url, f)
    f.seek(0)
    EnsureDirExists(output_dir)
    if url.endswith('.zip') or is_known_zip:
      assert path_prefixes is None
      zipfile.ZipFile(f).extractall(path=output_dir)
    else:
      t = tarfile.open(mode='r:*', fileobj=f)
      members = None
      if path_prefixes is not None:
        members = [m for m in t.getmembers()
                   if any(m.name.startswith(p) for p in path_prefixes)]
      t.extractall(path=output_dir, members=members)


def GetPlatformUrlPrefix(host_os):
  _HOST_OS_URL_MAP = {
      'linux': 'Linux_x64',
      'mac': 'Mac',
      'mac-arm64': 'Mac_arm64',
      'win': 'Win',
  }
  return CDS_URL + '/' + _HOST_OS_URL_MAP[host_os] + '/'


def DownloadAndUnpackPackage(package_file,
                             output_dir,
                             host_os,
                             version=PACKAGE_VERSION):
  cds_file = "%s-%s.tar.xz" % (package_file, version)
  cds_full_url = GetPlatformUrlPrefix(host_os) + cds_file
  try:
    DownloadAndUnpack(cds_full_url, output_dir)
  except urllib.error.URLError:
    print('Failed to download prebuilt clang package %s' % cds_file)
    print('Use build.py if you want to build locally.')
    print('Exiting.')
    sys.exit(1)


def DownloadAndUnpackClangMacRuntime(output_dir):
  cds_file = "clang-mac-runtime-library-%s.tar.xz" % PACKAGE_VERSION
  # We run this only for the runtime libraries, and 'mac' and 'mac-arm64' both
  # have the same (universal) runtime libraries. It doesn't matter which one
  # we download here.
  cds_full_url = GetPlatformUrlPrefix('mac') + cds_file
  try:
    DownloadAndUnpack(cds_full_url, output_dir)
  except urllib.error.URLError:
    print('Failed to download prebuilt clang %s' % cds_file)
    print('Use build.py if you want to build locally.')
    print('Exiting.')
    sys.exit(1)


def DownloadAndUnpackClangWinRuntime(output_dir):
  cds_file = "clang-win-runtime-library-%s.tar.xz" % PACKAGE_VERSION
  cds_full_url = GetPlatformUrlPrefix('win') + cds_file
  try:
    DownloadAndUnpack(cds_full_url, output_dir)
  except urllib.error.URLError:
    print('Failed to download prebuilt clang %s' % cds_file)
    print('Use build.py if you want to build locally.')
    print('Exiting.')
    sys.exit(1)


def UpdatePackage(package_name, host_os, dir=LLVM_BUILD_DIR):
  stamp_file = None
  package_file = None

  stamp_file = os.path.join(dir, package_name + '_revision')
  if package_name == 'clang':
    stamp_file = STAMP_FILE
    package_file = 'clang'
  elif package_name == 'coverage_tools':
    stamp_file = os.path.join(dir, 'cr_coverage_revision')
    package_file = 'llvm-code-coverage'
  elif package_name == 'objdump':
    package_file = 'llvmobjdump'
  elif package_name in ['clang-tidy', 'clangd', 'libclang', 'translation_unit']:
    package_file = package_name
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
  # This file is created by first class GCS deps. If this file exists,
  # clear the entire directory and download with this script instead.
  if glob.glob(os.path.join(dir, '.*_is_first_class_gcs')):
    RmTree(dir)
  elif ReadStampFile(stamp_file) == expected_stamp:
    return 0

  # Updating the main clang package nukes the output dir. Any other packages
  # need to be updated *after* the clang package.
  if package_name == 'clang' and os.path.exists(dir):
    RmTree(dir)

  DownloadAndUnpackPackage(package_file, dir, host_os)

  if package_name == 'clang' and 'mac' in target_os:
    DownloadAndUnpackClangMacRuntime(dir)
  if package_name == 'clang' and 'win' in target_os:
    # When doing win/cross builds on other hosts, get the Windows runtime
    # libraries, and llvm-symbolizer.exe (needed in asan builds).
    DownloadAndUnpackClangWinRuntime(dir)

  WriteStampFile(expected_stamp, stamp_file)
  return 0


def GetDefaultHostOs():
  _PLATFORM_HOST_OS_MAP = {
      'darwin': 'mac',
      'cygwin': 'win',
      'linux2': 'linux',
      'win32': 'win',
  }
  default_host_os = _PLATFORM_HOST_OS_MAP.get(sys.platform, sys.platform)
  if default_host_os == 'mac' and platform.machine() == 'arm64':
    default_host_os = 'mac-arm64'
  return default_host_os


def main():
  parser = argparse.ArgumentParser(description='Update clang.')
  parser.add_argument('--output-dir',
                      help='Where to extract the package.')
  parser.add_argument('--package',
                      help='What package to update (default: clang)',
                      default='clang')
  parser.add_argument('--host-os',
                      help=('Which host OS to download for '
                            '(default: %(default)s)'),
                      default=GetDefaultHostOs(),
                      choices=('linux', 'mac', 'mac-arm64', 'win'))
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

  if args.verify_version and args.verify_version != RELEASE_VERSION:
    print('RELEASE_VERSION is %s but --verify-version argument was %s.' % (
        RELEASE_VERSION, args.verify_version))
    print('clang_version in build/toolchain/toolchain.gni is likely outdated.')
    return 1

  if args.print_clang_version:
    print(RELEASE_VERSION)
    return 0

  output_dir = LLVM_BUILD_DIR
  if args.output_dir:
    global STAMP_FILE
    output_dir = os.path.abspath(args.output_dir)
    STAMP_FILE = os.path.join(output_dir, STAMP_FILENAME)

  if args.print_revision:
    if args.llvm_force_head_revision:
      force_head_revision = ReadStampFile(FORCE_HEAD_REVISION_FILE)
      if force_head_revision == '':
        print('No locally built version found!')
        return 1
      print(force_head_revision)
      return 0

    stamp_version = ReadStampFile(STAMP_FILE).partition(',')[0]
    if PACKAGE_VERSION != stamp_version:
      print('The expected clang version is %s but the actual version is %s' %
            (PACKAGE_VERSION, stamp_version))
      print('Did you run "gclient sync"?')
      return 1

    print(PACKAGE_VERSION)
    return 0

  if args.llvm_force_head_revision:
    print('--llvm-force-head-revision can only be used for --print-revision')
    return 1

  return UpdatePackage(args.package, args.host_os, output_dir)


if __name__ == '__main__':
  sys.exit(main())
