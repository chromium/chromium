#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
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

RUNTIME_SIGNATURE_FILE = '.version'
WEB_RUNTIME_ROOT = os.path.abspath(
    os.path.join(DIR_SOURCE_ROOT, 'third_party', 'cast_web_runtime'))
ZIP_PATH_TEMPLATE = (
    'gs://gtv-eureka/internal/master/core_runtime-eng/{version}' \
    '/core_runtime_package.zip')


# Fetches a .zip file from GCS and uncompresses it to |output_dir|.
def DownloadAndUnpackFromCloudStorage(url, output_dir):
  with tempfile.TemporaryDirectory() as tmpdir:
    temp_zip_file = os.path.join(tmpdir, 'web_runtime.zip')
    cmd = [
        os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'), 'cp', url,
        temp_zip_file
    ]
    task = subprocess.check_call(cmd,
                                 stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL)
    with ZipFile(temp_zip_file, 'r') as zip_ref:
      zip_ref.extractall(output_dir)


def MakeCleanDirectory(directory_name):
  if os.path.exists(directory_name):
    shutil.rmtree(directory_name)
  os.mkdir(directory_name)


def main():
  runtime_version = open(
      os.path.join(os.path.dirname(__file__),
                   'runtime.version')).read().strip()
  signature_file_path = os.path.join(WEB_RUNTIME_ROOT, RUNTIME_SIGNATURE_FILE)
  current_signature = (open(signature_file_path, 'r').read().strip()
                       if os.path.exists(signature_file_path) else '')
  if current_signature != runtime_version:
    logging.info(
        'Downloading Cast Web Runtime version {}...'.format(runtime_version))
    MakeCleanDirectory(WEB_RUNTIME_ROOT)
    DownloadAndUnpackFromCloudStorage(
        ZIP_PATH_TEMPLATE.format(version=runtime_version), WEB_RUNTIME_ROOT)
  with open(signature_file_path, 'w') as f:
    f.write(runtime_version)
  return 0


if __name__ == '__main__':
  sys.exit(main())
