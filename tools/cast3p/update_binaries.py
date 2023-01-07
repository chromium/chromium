#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import shutil
import sys
import tempfile

from zipfile import ZipFile

DIR_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

sys.path.append(os.path.join(DIR_SOURCE_ROOT, 'build'))
import find_depot_tools

CAST_CORE_ROOT = os.path.abspath(
    os.path.join(DIR_SOURCE_ROOT, 'third_party', 'cast_core', 'prebuilts'))
CAST_CORE_ZIP_PATH_TEMPLATE = (
    'gs://castlite-release-artifacts/{version}/third_party/castlite' \
    '/cast_core_qa_sdk_runtime_vizio_castos_armv7a' \
    '/sdk_runtime_vizio_castos_armv7a.tgz')
SIGNATURE_FILE = '.version'
RUNTIME_ROOT = os.path.abspath(
    os.path.join(DIR_SOURCE_ROOT, 'third_party', 'cast_web_runtime'))
RUNTIME_ZIP_PATH_TEMPLATE = (
    'gs://gtv-eureka/internal/1.56core/core_runtime-eng/{version}' \
    '/core_runtime_package.zip')


def DownloadFromCloudStorage(url, output_dir):
  """Fetches a file from GCS and put it in |output_dir|."""
  cmd = [
      os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'), 'cp', url,
      output_dir
  ]
  task = subprocess.check_call(cmd,
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)


def MakeCleanDirectory(directory_name):
  if os.path.exists(directory_name):
    shutil.rmtree(directory_name)
  os.mkdir(directory_name)


def UpdateBinaryIfNecessary(name, version_file, output_dir, gcs_path_template):
  """Update the binary at |output_dir| if necessary by comparing versions."""
  version = open(os.path.join(os.path.dirname(__file__),
                              version_file)).read().strip()
  signature_file_path = os.path.join(output_dir, SIGNATURE_FILE)
  current_signature = (open(signature_file_path, 'r').read().strip()
                       if os.path.exists(signature_file_path) else '')
  if current_signature != version:
    logging.info('Downloading {} version {}...'.format(name, version))
    MakeCleanDirectory(output_dir)
    DownloadFromCloudStorage(gcs_path_template.format(version=version),
                             output_dir)
  with open(signature_file_path, 'w') as f:
    f.write(version)


def main():
  UpdateBinaryIfNecessary('Cast Core', 'cast_core.version', CAST_CORE_ROOT,
                          CAST_CORE_ZIP_PATH_TEMPLATE)
  UpdateBinaryIfNecessary('Cast Web Runtime', 'runtime.version', RUNTIME_ROOT,
                          RUNTIME_ZIP_PATH_TEMPLATE)
  return 0


if __name__ == '__main__':
  sys.exit(main())
