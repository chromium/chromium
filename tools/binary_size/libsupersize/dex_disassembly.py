# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the dex disassembly for symbols."""

import dataclasses
import difflib
import json
import logging
import os
import re
import subprocess
import sys
import zipfile

import archive
import models
import path_util
import zip_util

_PROGUARD_CLASS_MAPPING_RE = re.compile(r'(?P<original_name>[^ ]+)'
                                        r' -> '
                                        r'(?P<obfuscated_name>[^:]+):')
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
  cmd = [
      'java', '-cp', r8_path, 'com.android.tools.r8.Disassemble', '--pg-map',
      mapping, apk_path
  ]
  return subprocess.check_output(cmd, encoding='utf8')


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


def _CreateClassDeobfuscationMap(obfuscated_mapping):
  mapping = {}
  for line in obfuscated_mapping:
    # We are on a class name so add it to the class mapping.
    if not line.startswith(' '):
      match = _PROGUARD_CLASS_MAPPING_RE.search(line)
      if match:
        mapping[match.group('obfuscated_name')] = match.group('original_name')
  return mapping


def _ChangeObfusactedNames(disassembly, obfuscated_map):
  for _, value in disassembly.items():
    for method in value.methods:
      method.return_type = obfuscated_map.get(method.return_type,
                                              method.return_type)
      if method.param_types:
        for idx, param in enumerate(method.param_types):
          method.param_types[idx] = obfuscated_map.get(param, param)
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


def _CaptureDisassemblyForSymbol(symbol, apk_to_disassembly, apk_dir,
                                 deobfuscation_map):
  logging.debug('Attempting to capture disassembly for symbol %s',
                symbol.full_name)
  container = symbol.container
  proguard_mapping_file_name = container.metadata.get(
      'proguard_mapping_file_name')
  if proguard_mapping_file_name is None:
    logging.debug('Mapping file does not exist in container metadata.')
    return None
  proguard_mapping_file_path = os.path.join(apk_dir, proguard_mapping_file_name)
  apk_file_name = container.metadata.get('apk_file_name')
  apk_file_path = os.path.join(apk_dir, apk_file_name)
  split_name = container.metadata.get('apk_split_name')
  cache_key = (apk_file_path, split_name)
  disassembly = apk_to_disassembly.get(cache_key)
  if disassembly is None:
    if split_name:
      logging.debug('Running R8 on APK: %s', split_name)
      with zip_util.UnzipToTemp(apk_file_path,
                                f'splits/{split_name}.apk') as split_path:
        r8_output = _DisassembleApk(proguard_mapping_file_path, split_path)
    else:
      logging.debug('Running R8 on APK: %s', apk_file_name)
      r8_output = _DisassembleApk(proguard_mapping_file_path, apk_file_path)
    obfuscated_to_deobfuscated_class_names = deobfuscation_map.get(
        proguard_mapping_file_path)
    if obfuscated_to_deobfuscated_class_names is None:
      logging.debug('Parsing mapping file %s', proguard_mapping_file_path)
      with open(proguard_mapping_file_path, 'r') as fh:
        obfuscated_to_deobfuscated_class_names = _CreateClassDeobfuscationMap(
            fh)
      deobfuscation_map[
          proguard_mapping_file_path] = obfuscated_to_deobfuscated_class_names
    logging.debug('Changing obfuscated names...')
    disassembly = _ChangeObfusactedNames(
        _ParseDisassembly(r8_output), obfuscated_to_deobfuscated_class_names)
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


def _AddUnifiedDiff(top_changed_symbols, before_directory, after_directory):
  # Counter used to skip over symbols where we couldn't find the disassembly.
  counter = 10
  before = None
  after = None
  before_apk_to_disassembly = {}
  after_apk_to_disassembly = {}
  deobfuscation_map = {}
  for symbol in top_changed_symbols:
    logging.debug('Symbols to go: %d', counter)
    after = _CaptureDisassemblyForSymbol(symbol.after_symbol,
                                         after_apk_to_disassembly,
                                         after_directory, deobfuscation_map)
    if after is None:
      continue
    if symbol.before_symbol:
      before = _CaptureDisassemblyForSymbol(symbol.before_symbol,
                                            before_apk_to_disassembly,
                                            before_directory, deobfuscation_map)
    else:
      before = None
    logging.debug('Adding disassembly for symbol: %s ', symbol.full_name)
    symbol.disassembly = _CreateUnifiedDiff(symbol.full_name, before or [],
                                            after)
    counter -= 1
    if counter == 0:
      break


def _GetTopChangedSymbols(delta_size_info):
  # Currently we are restricting symbols to dex methods.
  sorted_symbols = [
      symbol for symbol in delta_size_info.raw_symbols
      if symbol.section_name.endswith('dex.method') and symbol.after_symbol
  ]
  sorted_symbols.sort(key=lambda x: -abs(x.size))
  return sorted_symbols


def AddDisassembly(delta_size_info, before_directory, after_directory):
  """Adds disassembly diffs to top changed symbols.

    Adds the unified diff on the "before" and "after" disassembly to the
    top 10 changed symbols.

    Args:
      delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
      before_directory: Directory of the "before" APK.
      after_directory: Directory of the "after" APK.
  """
  logging.debug('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(delta_size_info)
  logging.debug('Adding disassembly to top 10 changed symbols')
  _AddUnifiedDiff(top_changed_symbols, before_directory, after_directory)


def main():
  # Gets disassembly for symbol.
  size_file_path = sys.argv[1]
  symbol_full_name = sys.argv[2]
  mapping_file_name = sys.argv[3]
  apk_dir = sys.argv[4]
  size_info = archive.LoadAndPostProcessSizeInfo(size_file_path)
  matched_symbols = [
      sym for sym in size_info.raw_symbols if sym.full_name == symbol_full_name
  ]
  if not matched_symbols:
    print(f'Symbol {symbol_full_name} not found')
    return
  for i, sym in enumerate(matched_symbols):
    if i > 0:
      print('-' * 80)
    sym.container.metadata[
        models.METADATA_PROGUARD_MAPPING_FILENAME] = mapping_file_name
    bytecode = _CaptureDisassemblyForSymbol(sym, {}, apk_dir, {})
    for line in bytecode:
      print(line, end='')
  return


if __name__ == '__main__':
  main()
