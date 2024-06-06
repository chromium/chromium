#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Lint as: python3
"""Creates files required to feed into trybot_commit_size_checker"""

import argparse
import json
import logging
import os
import shutil
import subprocess

_SRC_ROOT = os.path.normpath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
_RESOURCE_SIZES_PATH = os.path.join(_SRC_ROOT, 'build', 'android',
                                    'resource_sizes.py')
_BINARY_SIZE_DIR = os.path.join(_SRC_ROOT, 'tools', 'binary_size')
_CLANG_UPDATE_PATH = os.path.join(_SRC_ROOT, 'tools', 'clang', 'scripts',
                                  'update.py')


def _copy_files_to_staging_dir(files_to_copy, make_staging_path):
  """Copies files from output directory to staging_dir"""
  for filename in files_to_copy:
    shutil.copy(filename, make_staging_path(filename))


def _generate_resource_sizes(to_resource_sizes_py, make_chromium_output_path,
                             make_staging_path, filename):
  """Creates results-chart.json as $staging_dir/$filename"""
  cmd = [
      _RESOURCE_SIZES_PATH,
      make_chromium_output_path(to_resource_sizes_py['apk_name']),
      '--output-format=chartjson',
      '--chromium-output-directory',
      make_chromium_output_path(),
      '--output-dir',
      make_staging_path(),
  ]
  FORWARDED_PARAMS = [
      ('--trichrome-library', make_chromium_output_path, 'trichrome_library'),
      ('--trichrome-chrome', make_chromium_output_path, 'trichrome_chrome'),
      ('--trichrome-webview', make_chromium_output_path, 'trichrome_webview'),
  ]
  for switch, fun, key in FORWARDED_PARAMS:
    if key in to_resource_sizes_py:
      cmd += [switch, fun(to_resource_sizes_py[key])]
  subprocess.run(cmd, check=True)
  os.rename(make_staging_path('results-chart.json'),
            make_staging_path(filename))


def _generate_supersize_archive(supersize_input_file, make_chromium_output_path,
                                make_staging_path):
  """Creates a .size file for the given .apk or .minimal.apks"""
  supersize_input_path = make_chromium_output_path(supersize_input_file)
  size_path = make_staging_path(supersize_input_file) + '.size'

  supersize_script_path = os.path.join(_BINARY_SIZE_DIR, 'supersize')

  subprocess.run(
      [
          supersize_script_path,
          'archive',
          size_path,
          '-f',
          supersize_input_path,
          '-v',
      ],
      check=True,
  )


def main():
  parser = argparse.ArgumentParser()

  # Schema for android_size_bot_config:
  #   name: The name of the path to the generated size config JSON file.
  #   archive_files: List of files to archive after building, and make available
  #     to trybot_commit_size_checker.py.
  #   mapping_files: A list of .mapping files.
  #     Used by trybot_commit_size_checker.py to look for ForTesting symbols.
  #   supersize_input_file: Main input for SuperSize, and can be {.apk,
  #     .minimal.apks, .ssargs}.
  #   to_resource_sizes_py: Scope containing data to pass to resource_sizes.py.
  #      Its fields are:
  #        * resource_size_args: A dict of arguments for resource_sizes.py. Its
  #          sub-fields are:
  #          * apk_name: Required main input, although for Trichrome this can be
  #            a placeholder name.
  #          * trichrome_library: --trichrome-library param (Trichrome only).
  #          * trichrome_chrome: --trichrome-chrome param (Trichrome only).
  #          * trichrome_webview: --trichrome-webview param (Trichrome only).
  #        * supersize_input_file: Main input for SuperSize.

  parser.add_argument('--size-config-json',
                      required=True,
                      help='Path to android_size_bot_config JSON')
  parser.add_argument('--chromium-output-directory',
                      required=True,
                      help='Location of the build artifacts.')
  parser.add_argument('--staging-dir',
                      required=True,
                      help='Directory to write generated files to.')
  args = parser.parse_args()

  with open(args.size_config_json, 'rt') as fh:
    config = json.load(fh)
  mapping_files = config['mapping_files']
  supersize_input_file = config['supersize_input_file']
  # TODO(agrieve): Remove fallback to mapping_files once archive_files is added
  #     to all files.
  archive_files = config.get('archive_files', mapping_files)

  def make_chromium_output_path(path_rel_to_output=None):
    if path_rel_to_output is None:
      return args.chromium_output_directory
    return os.path.join(args.chromium_output_directory, path_rel_to_output)

  # N.B. os.path.basename() usage.
  def make_staging_path(path_rel_to_output=None):
    if path_rel_to_output is None:
      return args.staging_dir
    return os.path.join(args.staging_dir, os.path.basename(path_rel_to_output))

  files_to_copy = [make_chromium_output_path(f) for f in archive_files]

  # Copy size config JSON to staging dir to save settings used.
  if args.size_config_json:
    files_to_copy.append(args.size_config_json)
  _copy_files_to_staging_dir(files_to_copy, make_staging_path)

  config_32 = config['to_resource_sizes_py']
  _generate_resource_sizes(config_32, make_chromium_output_path,
                           make_staging_path, 'resource_sizes_32.json')
  config_64 = config.get('to_resource_sizes_py_64')
  if config_64:
    _generate_resource_sizes(config_64, make_chromium_output_path,
                             make_staging_path, 'resource_sizes_64.json')

  _generate_supersize_archive(supersize_input_file, make_chromium_output_path,
                              make_staging_path)


if __name__ == '__main__':
  main()
