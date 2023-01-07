#!/usr/bin/env python3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Script to build a CIPD package for weblayer_instrumentation_test_apk from
# the current Chromium checkout.
#
# This should be run from the src directory of a release branch. This will
# take care of the build, you need not do that yourself. After the package is
# built run two cipd commands (printed at the end of script execution) to
# upload the package to the CIPD server and to update the ref for the
# corresponding milestone. Once the ref is updated, the version skew test will
# pick up the new package in successive runs.

import argparse
import contextlib
import os
import shutil
import subprocess
import sys
import re
import tempfile
import zipfile

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..')

# Run mb.py out of the current branch for simplicity.
MB_PATH = os.path.join('tools', 'mb', 'mb.py')

# Get the config specifying the gn args from the location of this script.
MB_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              'mb_config.pyl')

CHROMIUM_VERSION_REGEX = r'\d+\.\d+\.\d+\.\d+$'

# CIPD package path.
# https://chrome-infra-packages.appspot.com/p/chromium/testing/weblayer-x86/+/
CIPD_PKG_PATH='chromium/testing/weblayer-x86'


@contextlib.contextmanager
def temporarily_chdir_to_src(local_paths=None):
  """Change directories to chromium/src when entering with block and
  then change back to current directory after exiting with block.

  Args:
    local_paths: List of paths to change into relative paths.

  Returns:
    List of paths relative to chromium/src.
  """
  curr_dir = os.getcwd()
  paths_rel_to_src = [
      os.path.relpath(p, SRC_DIR) for p in local_paths or []]
  try:
    os.chdir(SRC_DIR)
    yield paths_rel_to_src
  finally:
    os.chdir(curr_dir)


def zip_test_target(zip_filename):
  """Create zip of all deps for weblayer_instrumentation_test_apk.

  Args:
    zip_filename: destination zip filename.
  """
  cmd = [MB_PATH,
         'zip',
         '--master=dummy.master',
         '--builder=dummy.builder',
         '--config-file=%s' % MB_CONFIG_PATH,
         os.path.join('out', 'Release'),
         'weblayer_instrumentation_test_apk',
         zip_filename]
  print(' '.join(cmd))
  subprocess.check_call(cmd)


def build_cipd_pkg(input_path, cipd_filename):
  """Create a CIPD package file from the given input path.

  Args:
    input_path: input directory from which to build the package.
    cipd_filename: output filename for resulting cipd archive.
  """
  cmd = ['cipd',
         'pkg-build',
         '--in=%s' % input_path,
         '--install-mode=copy',
         '--name=%s' % CIPD_PKG_PATH,
         '--out=%s' % cipd_filename]
  print(' '.join(cmd))
  subprocess.check_call(cmd)


def get_chromium_version():
  with open(os.path.join(SRC_DIR, 'chrome', 'VERSION')) as f:
    version = '.'.join(line[line.index('=') + 1:]
                       for line in f.read().splitlines())
  if not re.match(CHROMIUM_VERSION_REGEX, version):
    raise ValueError("Chromium version, '%s', is not in proper format" %
                     version)
  return version


def main():
  parser = argparse.ArgumentParser(
      description='Package weblayer instrumentation tests for CIPD.')
  parser.add_argument(
      '--cipd_out',
      required=True,
      help="Output filename for resulting .cipd file.")

  args = parser.parse_args()
  chromium_version = get_chromium_version()
  with tempfile.TemporaryDirectory() as tmp_dir, \
       temporarily_chdir_to_src([args.cipd_out]) as cipd_out_src_rel_paths:
    # Create zip archive of test target.
    zip_filename = os.path.join(tmp_dir, 'file.zip')
    zip_test_target(zip_filename)

    # Extract zip archive.
    extracted = os.path.join(tmp_dir, 'extracted')
    os.mkdir(extracted)
    with zipfile.ZipFile(zip_filename) as zip_file:
      zip_file.extractall(path=extracted)

    # Create CIPD archive.
    tmp_cipd_filename = os.path.join(tmp_dir, 'file.cipd')
    build_cipd_pkg(extracted, tmp_cipd_filename)
    shutil.move(tmp_cipd_filename, cipd_out_src_rel_paths[0])

    print(('Use "cipd pkg-register %s -verbose -tag \'version:%s\'" ' +
           'to upload package to the cipd server.') %
          (args.cipd_out, chromium_version))
    print('Use "cipd set-ref chromium/testing/weblayer-x86 --version ' +
          '<CIPD instance version> -ref m<milestone>" to update the ref.')
    print('The CIPD instance version can be found on the "Instance" line ' +
          'after "chromium/testing/weblayer-x86:".')


if __name__ == '__main__':
  sys.exit(main())
