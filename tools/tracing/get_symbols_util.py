# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import breakpad_file_extractor
import flag_utils


def MangleModuleIfNeeded(module_id):
  """Convert module ID to the format breakpad uses.

  See TracingSamplerProfiler::MangleModuleIDIfNeeded().
  Warning: this is only relevant in Android, Linux and CrOS.
  Linux ELF module IDs are 160bit integers, which we need to mangle
  down to 128bit integers to match the id that Breakpad outputs.
  Example on version '66.0.3359.170' x64:
     Build-ID: "7f0715c2 86f8 b16c 10e4ad349cda3b9b 56c7a773"
     Debug-ID  "C215077F F886 6CB1 10E4AD349CDA3B9B 0"

  Args:
    module_id: The module ID provided by crash reports.

  Returns:
    The module ID in breakpad format.
  """
  if len(module_id) == 32 or len(module_id) == 33:
    return module_id

  if len(module_id) < 32:
    return module_id.ljust(32, '0').upper()

  return ''.join([
      module_id[6:8], module_id[4:6], module_id[2:4], module_id[0:2],
      module_id[10:12], module_id[8:10], module_id[14:16], module_id[12:14],
      module_id[16:32], '0'
  ]).upper()


def FindMatchingModule(symbol_root_dir, dump_syms_path, module_id):
  """Finds the first matching module with the given ID from symbol dir.

  Searches through all the subdirectories of the symbol directory.
  Args:
    symbol_root_dir: The root directory with all the unzipped symbol files.
    dump_syms_path: Path to dupm_syms binary.
    module_id: The module ID of the binary to find.

  Returns:
    Absolute path to the binary with the given module ID, or None if not found.
  """
  logger = flag_utils.GetTracingLogger()
  modules = {MangleModuleIfNeeded(module_id)}
  logger.debug('Mangled module ID: %s', str(modules))

  for root_dir, _, _ in os.walk(symbol_root_dir, topdown=True):
    root_path = os.path.abspath(root_dir)
    for file_iter in os.listdir(root_path):
      input_file_path = os.path.join(root_path, file_iter)
      if not os.path.isfile(
          input_file_path) or not breakpad_file_extractor.IsValidBinaryPath(
              input_file_path):
        continue

      logger.debug('Checking for match with %s', input_file_path)
      if not breakpad_file_extractor.IsModuleNeededForSymbolization(
          dump_syms_path, modules, input_file_path):
        continue

      logger.info('Found module path %s', input_file_path)
      return input_file_path
  return None
