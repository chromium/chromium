#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Lint as: python3
"""Creates files required to feed into trybot_commit_size_checker"""

import argparse
import os
import logging
import shutil
import subprocess

_SRC_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
_RESOURCE_SIZES_PATH = os.path.join(_SRC_ROOT, 'build', 'android',
                                    'resource_sizes.py')
_BINARY_SIZE_DIR = os.path.join(_SRC_ROOT, 'tools', 'binary_size')
_CLANG_UPDATE_PATH = os.path.join(_SRC_ROOT, 'tools', 'clang', 'scripts',
                                  'update.py')


def _extract_proguard_mapping(apk_name, mapping_name_list, staging_dir,
                              chromium_output_directory):
  """Copies proguard mapping files to staging_dir"""
  for mapping_name in mapping_name_list:
    mapping_path = os.path.join(chromium_output_directory, 'apks', mapping_name)
    shutil.copy(mapping_path, os.path.join(staging_dir, apk_name + '.mapping'))


def _generate_resource_sizes(apk_name, staging_dir, chromium_output_directory):
  """Creates results-chart.json file in staging_dir"""
  apk_path = os.path.join(chromium_output_directory, 'apks', apk_name)

  subprocess.run(
      [
          _RESOURCE_SIZES_PATH,
          apk_path,
          '--output-format=chartjson',
          '--output-dir',
          staging_dir,
          '--chromium-output-directory',
          chromium_output_directory,
      ],
      check=True,
  )


def _generate_supersize_archive(apk_name, staging_dir,
                                chromium_output_directory):
  """Creates a .size file for the given .apk or .minimal.apks"""
  subprocess.run([_CLANG_UPDATE_PATH, '--package=objdump'], check=True)
  apk_path = os.path.join(chromium_output_directory, 'apks', apk_name)
  size_path = os.path.join(staging_dir, apk_name + '.size')

  supersize_script_path = os.path.join(_BINARY_SIZE_DIR, 'supersize')

  subprocess.run(
      [
          supersize_script_path,
          'archive',
          size_path,
          '-f',
          apk_path,
          '-v',
      ],
      check=True,
  )


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--apk-name',
      required=True,
      help='Name of the apk (ex. Name.apk)',
  )
  parser.add_argument(
      '--chromium-output-directory',
      required=True,
      help='Location of the build artifacts.',
  )
  parser.add_argument(
      '--mapping-name',
      required=True,
      action='append',
      help='Filename of the proguard mapping file.',
  )
  parser.add_argument(
      '--staging-dir',
      required=True,
      help='Directory to write generated files to.',
  )

  args = parser.parse_args()

  _extract_proguard_mapping(
      args.apk_name,
      args.mapping_name,
      args.staging_dir,
      args.chromium_output_directory,
  )
  _generate_resource_sizes(
      args.apk_name,
      args.staging_dir,
      args.chromium_output_directory,
  )
  _generate_supersize_archive(
      args.apk_name,
      args.staging_dir,
      args.chromium_output_directory,
  )


if __name__ == '__main__':
  main()
