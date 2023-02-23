#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This is a script helping developers upload web-engine artifacts.
#
# `python3 tools/fuchsia/upload_webengine_build.py -c <commit> -d <build dir>
#      -g <GCS path>`
# The script will sync, compile, and upload relevant artifacts to the GCS dir.

import argparse
import os
import posixpath
import subprocess
import sys

WEB_ENGINE_TARGETS = [
    'cast_runner_pkg', 'web_engine_shell_pkg',
    'performance_web_engine_test_suite'
]

FAR_ARTIFACTS = ['cast_runner.far', 'web_engine_shell.far', 'web_engine.far']


def Run(command):
  print(command)
  subprocess.run(command,
                 shell=True,
                 check=True,
                 encoding='utf-8',
                 stderr=subprocess.STDOUT)


def FindFile(file, root_dir):
  for root, _, files in os.walk(root_dir):
    for f in files:
      if file == f:
        return os.path.join(root, f)

  raise AssertionError(
      f'File {file} not found. Be sure it is created by one of the '
      f'targets {WEB_ENGINE_TARGETS}')


def StartBuildAndUpload(out_dir, gcs_path, commit=None):
  if commit:
    Run(f'git checkout {commit}')
  Run('gclient sync')

  all_targets = ' '.join(WEB_ENGINE_TARGETS)
  Run(f'autoninja -C {out_dir} {all_targets}')

  # Now to find the relevant far files.
  full_paths = [FindFile(artifact, out_dir) for artifact in FAR_ARTIFACTS]
  for path in full_paths:
    # Specifying the exact name is helpful when the GCS path does not end in a
    # '/', which can cause `gsutil` to interpret it as the destination file and
    # not as a directory.
    file = os.path.basename(path)
    remote_path = posixpath.join(gcs_path, file)
    Run(f'gsutil -m cp {path} {remote_path}')

  return 0


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-c',
                      '--commit',
                      type=str,
                      help='Revision to checkout, compile, and upload.',
                      required=False)
  parser.add_argument('-g',
                      '--gcs_path',
                      type=str,
                      help='A full GCS path that will contain the artifacts.',
                      required=True)
  parser.add_argument('-d',
                      '--out_dir',
                      type=str,
                      help=('Output directory to build from. '
                            'Must have GN args set already.'),
                      required=True)
  args = parser.parse_args()
  return StartBuildAndUpload(args.out_dir, args.gcs_path, args.commit)


if __name__ == '__main__':
  sys.exit(main())
