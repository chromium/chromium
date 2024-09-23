#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the dex disassembly for symbols."""

import argparse
import code
import difflib
import functools
import io
import logging
import os
import re
import readline  # Makes code.InteractiveConsole works better.
import subprocess
import sys
import tempfile
import zipfile

import r8_disassembly
import path_util
import zip_util

_DISASSEMBLED_METHOD_QUOTA = 10
_SYMBOL_FULL_NAME_RE = re.compile(r'(.*?)#(.*?)\((.*?)\):? ?(.*)')


class _CachedApkDisassembler:
  def __init__(self):
    self._proguard_mapping_file_path_lookup = {}

  def AssignProguardMappingPath(self, apk_file_path,
                                proguard_mapping_file_path):
    self._proguard_mapping_file_path_lookup[apk_file_path] = (
        proguard_mapping_file_path)

  def _DisassembleApk(self, mapping, apk_path):
    r8_path = path_util.GetR8Path()
    r8_output = None
    # Temporary hack until next R8 roll:
    # Prevents R8 failing due to assets/webapk7.dex.
    with tempfile.NamedTemporaryFile(mode='wb', suffix='.apk') as tmp_file:
      with zipfile.ZipFile(tmp_file, 'w') as dst_zip:
        with zipfile.ZipFile(apk_path) as src_zip:
          for info in src_zip.infolist():
            if info.filename.startswith('classes'):
              dst_zip.writestr(info, src_zip.read(info))
      tmp_file.flush()

      cmd = [
          path_util.GetJavaExec(), '-cp', r8_path,
          'com.android.tools.r8.Disassemble', '--pg-map', mapping, tmp_file.name
      ]
      try:
        r8_output = subprocess.check_output(cmd, encoding='utf-8')
      except subprocess.CalledProcessError:
        logging.debug('Running R8 failed on APK: %s', apk_path)

    return r8_output

  @functools.lru_cache(None)
  def GetForApkAndSplit(self, apk_file_path, split_name):
    proguard_mapping_file_path = (
        self._proguard_mapping_file_path_lookup[apk_file_path])
    r8_output = None
    if split_name:
      logging.info('Creating disassembly for APK split: %s', split_name)
      with zip_util.UnzipToTemp(
          apk_file_path, f'splits/{split_name}-master.apk') as split_path:
        r8_output = self._DisassembleApk(proguard_mapping_file_path, split_path)
    elif apk_file_path.endswith('.apk'):
      logging.info('Creating disassembly for APK: %s', apk_file_path)
      r8_output = self._DisassembleApk(proguard_mapping_file_path,
                                       apk_file_path)
    if r8_output is None:
      return None
    class_obj_map, _ = r8_disassembly.Parse(io.StringIO(r8_output))
    return class_obj_map


def CreateCache():
  return _CachedApkDisassembler()


def _ExtractDisassemblyForMethod(class_obj_map, method):
  param_types = None
  return_type = None
  bytecode = None
  # Example of method:
  # className#methodName(param1,param2): returnType
  m = _SYMBOL_FULL_NAME_RE.match(method)
  if m:
    class_name, method_name, param_types, return_type = m.groups()
    param_types = param_types.split(',') if param_types else []
    class_obj = class_obj_map.get(class_name)
    if class_obj is not None:
      bytecode = class_obj.FindMethodByteCode(class_name, method_name,
                                              param_types, return_type)
  return bytecode


def Disassemble(symbol, path_resolver, apk_disassembler_cache):
  logging.debug('Disassembling %s', symbol.full_name)
  container = symbol.container
  proguard_mapping_file_name = container.metadata.get(
      'proguard_mapping_file_name')
  if proguard_mapping_file_name is None:
    raise Exception('Mapping file does not exist in container metadata.')

  proguard_mapping_file_path = path_resolver(proguard_mapping_file_name)
  apk_file_name = container.metadata['apk_file_name']
  apk_file_path = str(path_resolver(apk_file_name))
  split_name = container.metadata.get('apk_split_name')  # Can be None.
  apk_disassembler_cache.AssignProguardMappingPath(apk_file_path,
                                                   proguard_mapping_file_path)
  class_obj_map = apk_disassembler_cache.GetForApkAndSplit(
      apk_file_path, split_name)
  if class_obj_map is None:
    return None
  logging.info('Looking up disassembly for %s', symbol.full_name)
  return _ExtractDisassemblyForMethod(class_obj_map, symbol.full_name)


def _CreateUnifiedDiff(name, before, after):
  unified_diff = difflib.unified_diff(before,
                                      after,
                                      fromfile=name,
                                      tofile=name,
                                      n=10)
  # Strip new line characters as difflib.unified_diff adds extra newline
  # characters to the first few lines which we do not want.
  #unified_diff = [x.strip() for x in unified_diff]
  return ''.join(unified_diff)


def _AddUnifiedDiff(top_changed_symbols, before_path_resolver,
                    after_path_resolver):
  # Counter used to skip over symbols where we couldn't find the disassembly.
  counter = _DISASSEMBLED_METHOD_QUOTA
  before = None
  after = None
  after_apk_disassembler_cache = _CachedApkDisassembler()
  before_apk_disassembler_cache = _CachedApkDisassembler()
  for symbol in top_changed_symbols:
    logging.debug('Symbols to go: %d', counter)
    after = Disassemble(symbol.after_symbol, after_path_resolver,
                        after_apk_disassembler_cache)
    if after is None:
      continue
    if symbol.before_symbol:
      before = Disassemble(symbol.before_symbol, before_path_resolver,
                           before_apk_disassembler_cache)
    else:
      before = None
    logging.info('Adding disassembly for: %s', symbol.full_name)
    symbol.after_symbol.disassembly = _CreateUnifiedDiff(
        symbol.full_name, before or [], after)
    counter -= 1
    if counter == 0:
      break


def _GetTopChangedSymbols(delta_size_info):
  def filter_symbol(symbol):
    # We are only looking for symbols where the after_symbol exists, as
    # if it does not exist it does not provide much value in a side
    # by side code breakdown.
    if not symbol.after_symbol:
      return False
    # Currently restricting the symbols to .dex.method symbols only.
    if not symbol.section_name.endswith('dex.method'):
      return False
    # Symbols which have changed under 10 bytes do not add much value.
    if abs(symbol.pss) < 10:
      return False
    return True

  return delta_size_info.raw_symbols.Filter(filter_symbol).Sorted()


def AddDisassembly(delta_size_info, before_path_resolver, after_path_resolver):
  """Adds disassembly diffs to top changed dex symbols.

    Adds the unified diff on the "before" and "after" disassembly to the
    top |_DISASSEMBLED_METHOD_QUOTA| changed symbols.

    Args:
      delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
      before_path_resolver: Callable to compute paths for "before" artifacts.
      after_path_resolver: Callable to compute paths for "after" artifacts.
  """
  logging.info('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(delta_size_info)
  logging.info('Adding disassembly to top %d changed dex symbols',
               _DISASSEMBLED_METHOD_QUOTA)
  _AddUnifiedDiff(top_changed_symbols, before_path_resolver,
                  after_path_resolver)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('apk_path', type=str)
  parser.add_argument('mapping_file_path', type=str)
  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG)

  logging.info('Loading %s and %s...', args.apk_path, args.mapping_file_path)
  apk_disassembler_cache = _CachedApkDisassembler()
  apk_disassembler_cache.AssignProguardMappingPath(args.apk_path,
                                                   args.mapping_file_path)
  class_obj_map = apk_disassembler_cache.GetForApkAndSplit(args.apk_path, None)
  variables = {'class_obj_map': class_obj_map}
  banner = []
  banner.append('=' * 80)
  banner.append('class_obj_map: {method: archive_util.DexClass obj}')
  code.InteractiveConsole(variables).interact('\n'.join(banner))


if __name__ == '__main__':
  main()
