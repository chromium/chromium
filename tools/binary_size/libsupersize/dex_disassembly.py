#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the dex disassembly for symbols."""

import dataclasses
import difflib
import json
import logging
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import zipfile

import archive
import dex_deobfuscate
import models
import path_util
import zip_util

_SYMBOL_FULL_NAME_RE = re.compile(r'(.*?)#(.*?)\((.*?)\):? ?(.*)')
_R8_OUTPUT_RE = re.compile(r"'([^']+)'")
_R8_PARAM_RE = re.compile(r'\(.*?\)')


@dataclasses.dataclass
class _Method:
  name: str
  class_name: str
  param_types: list = None
  return_type: str = None
  bytecode: list = None


class _Class:
  def __init__(self, name):
    self.name = name
    self.methods = []

  def _FindMethodByteCode(self, class_name, method_name, param_types,
                          return_type):
    for method in self.methods:
      if (method.name == method_name and method.class_name == class_name
          and method.return_type == return_type
          and method.param_types == param_types):
        return method.bytecode
    return None


def _DisassembleApk(mapping, apk_path):
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
        'java', '-cp', r8_path, 'com.android.tools.r8.Disassemble', '--pg-map',
        mapping, tmp_file.name
    ]
    try:
      r8_output = subprocess.check_output(cmd, encoding='utf8')
    except subprocess.CalledProcessError:
      logging.debug('Running R8 failed on APK: %s', apk_path)

  return r8_output


def _ParseDisassembly(disassembly):
  disassembly = disassembly.splitlines(keepends=True)
  classes = {}
  current_class = None
  current_method = None
  reading_positions = False
  reading_methods = False
  for idx, line in enumerate(disassembly):
    line = line.strip()
    if line.startswith('# Class:'):
      # New class started, no longer reading methods.
      reading_methods = False
      # Example of line: '# Class: \'className\''
      match = _R8_OUTPUT_RE.search(line)
      if match:
        current_class = _Class(match.group(1))
        classes[current_class.name] = current_class
    elif line.startswith('# Method:') and current_class:
      # Example of line: '# Method: \'methodName\':'
      match = _R8_OUTPUT_RE.search(line)
      if match:
        reading_methods = True
        current_method = _Method(match.group(1), current_class.name)
        current_class.methods.append(current_method)
    elif reading_methods and line.startswith('registers:'):
      # Example of line: 'registers: 1, inputs: 1, outputs: 1'
      assert idx > 0
      header = disassembly[idx - 1]
      # Example of header: 'returnType className.methodName(param1, param2)'
      return_type = header.split()[0]
      current_method.return_type = return_type
      match = _R8_PARAM_RE.search(header)
      if match:
        params = (match.group(0))[1:-1]
        current_method.param_types = params.split(', ') if params else []
        bytecode_start = idx
        reading_positions = True
    elif reading_positions and line == '':
      current_method.bytecode = disassembly[bytecode_start:idx]
      reading_positions = False
  return classes


def _ChangeObfusactedNames(disassembly, class_deobfuscate_map):
  for _, value in disassembly.items():
    for method in value.methods:
      method.return_type = class_deobfuscate_map.get(method.return_type,
                                                     method.return_type)
      if method.param_types:
        for idx, param in enumerate(method.param_types):
          method.param_types[idx] = class_deobfuscate_map.get(param, param)
  return disassembly


def _ComputeDisassemblyForSymbol(deobfuscated_disassembly, symbol_full_name):
  param_types = None
  return_type = None
  bytecode = None
  # Example of symbol_full_name:
  # className#methodName(param1,param2): returnType
  m = _SYMBOL_FULL_NAME_RE.match(symbol_full_name)
  if m:
    class_name, method_name, param_types, return_type = m.groups()
    param_types = param_types.split(',') if param_types else []
    disassembly = deobfuscated_disassembly.get(class_name)
    if disassembly is not None:
      bytecode = disassembly._FindMethodByteCode(class_name, method_name,
                                                 param_types, return_type)
  return bytecode


def _CaptureDisassemblyForSymbol(symbol, apk_to_disassembly, path_resolver,
                                 dex_deobfuscator_cache):
  logging.debug('Attempting to capture disassembly for symbol %s',
                symbol.full_name)
  container = symbol.container
  proguard_mapping_file_name = container.metadata.get(
      'proguard_mapping_file_name')
  if proguard_mapping_file_name is None:
    raise Exception('Mapping file does not exist in container metadata.')

  proguard_mapping_file_path = path_resolver(proguard_mapping_file_name)
  apk_file_name = container.metadata['apk_file_name']
  apk_file_path = path_resolver(apk_file_name)
  split_name = container.metadata.get('apk_split_name')
  cache_key = (apk_file_path, split_name)
  disassembly = apk_to_disassembly.get(cache_key)
  if disassembly is None:
    r8_output = None
    if split_name:
      logging.info('Creating disassmebly for APK split: %s', split_name)
      with zip_util.UnzipToTemp(
          apk_file_path, f'splits/{split_name}-master.apk') as split_path:
        r8_output = _DisassembleApk(proguard_mapping_file_path, split_path)
    elif apk_file_path.endswith('.apk'):
      logging.info('Creating disassmebly for APK: %s', apk_file_name)
      r8_output = _DisassembleApk(proguard_mapping_file_path, apk_file_path)
    if r8_output is None:
      return None
    class_deobfuscation_map = (
        dex_deobfuscator_cache.GetForMappingFile(proguard_mapping_file_path))
    logging.debug('Changing obfuscated names...')
    disassembly = _ChangeObfusactedNames(_ParseDisassembly(r8_output),
                                         class_deobfuscation_map)
    apk_to_disassembly[cache_key] = disassembly
  return _ComputeDisassemblyForSymbol(disassembly, symbol.full_name)


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
  counter = 10
  before = None
  after = None
  before_apk_to_disassembly = {}
  after_apk_to_disassembly = {}
  dex_deobfuscator_cache = dex_deobfuscate.CachedDexDeobfuscators()
  for symbol in top_changed_symbols:
    logging.debug('Symbols to go: %d', counter)
    after = _CaptureDisassemblyForSymbol(symbol.after_symbol,
                                         after_apk_to_disassembly,
                                         after_path_resolver,
                                         dex_deobfuscator_cache)
    if after is None:
      continue
    if symbol.before_symbol:
      before = _CaptureDisassemblyForSymbol(symbol.before_symbol,
                                            before_apk_to_disassembly,
                                            before_path_resolver,
                                            dex_deobfuscator_cache)
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
    top 10 changed symbols.

    Args:
      delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
      before_path_resolver: Callable to compute paths for "before" artifacts.
      after_path_resolver: Callable to compute paths for "after" artifacts.
  """
  logging.info('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(delta_size_info)
  logging.info('Adding disassembly to top 10 changed dex symbols')
  _AddUnifiedDiff(top_changed_symbols, before_path_resolver,
                  after_path_resolver)


def main():
  # Gets disassembly for symbol.
  size_file_path = sys.argv[1]
  symbol_full_name = sys.argv[2]
  mapping_file_name = sys.argv[3]
  apk_dir = sys.argv[4]
  path_resolver = lambda x: os.path.join(apk_dir, x)
  size_info = archive.LoadAndPostProcessSizeInfo(size_file_path)
  matched_symbols = [
      sym for sym in size_info.raw_symbols if sym.full_name == symbol_full_name
  ]
  if not matched_symbols:
    print(f'Symbol {symbol_full_name} not found')
    return

  dex_deobfuscator_cache = dex_deobfuscate.CachedDexDeobfuscators()
  for i, sym in enumerate(matched_symbols):
    if i > 0:
      print('-' * 80)
    sym.container.metadata[
        models.METADATA_PROGUARD_MAPPING_FILENAME] = mapping_file_name
    bytecode = _CaptureDisassemblyForSymbol(sym, {}, path_resolver,
                                            dex_deobfuscator_cache)
    for line in bytecode:
      print(line, end='')
  return


if __name__ == '__main__':
  main()
