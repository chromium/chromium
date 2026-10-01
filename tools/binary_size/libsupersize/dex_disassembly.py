#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the dex disassembly for symbols."""

import argparse
import code
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

import disassembly_util
import r8_disassembly
import path_util
import zip_util

# Keep in sync with third_party/r8/disassemble.py
_PREFIX_RE = re.compile(r'^\s*\d+:\s+(0x[0-9a-f]+):\s+')
_JUMP_TARGET_RE = re.compile(r'(0x[0-9a-f]+)\s+\([+-]\d+\)')
_TARGET_RE = re.compile(r'->\s+(0x[0-9a-f]+)')
_REGISTER_RE = re.compile(r'\b[vp]\d+\b')
_TRY_RANGE_RE = re.compile(r'\[(0x[0-9a-f]+)\s+\.\.\s+(0x[0-9a-f]+)\[')


def NormalizeLines(lines):
  """Normalizes disassembly lines to make them diff-friendly.

  Keep in sync with third_party/r8/disassemble.py.
  """
  # Pass 1: Collect target addresses
  target_addresses = set()
  for line in lines:
    for m in _JUMP_TARGET_RE.finditer(line):
      target_addresses.add(int(m.group(1), 16))
    for m in _TARGET_RE.finditer(line):
      target_addresses.add(int(m.group(1), 16))
    for m in _TRY_RANGE_RE.finditer(line):
      target_addresses.add(int(m.group(1), 16))
      target_addresses.add(int(m.group(2), 16))

  # Pass 2: Normalize and insert labels
  ret = []
  for line in lines:
    if 'PcBasedDebugInfo' in line or line.startswith('~~R8{'):
      continue
    m = _PREFIX_RE.match(line)
    if m:
      offset = int(m.group(1), 16)
      if offset in target_addresses:
        ret.append('<target>:\n')
      line = _PREFIX_RE.sub('', line)
      line = _REGISTER_RE.sub('vN', line)
      line = _JUMP_TARGET_RE.sub('<target>', line)
      line = _TARGET_RE.sub('-> <target>', line)
      ret.append(line.rstrip() + '\n')
    else:
      line = _JUMP_TARGET_RE.sub('<target>', line)
      line = _TARGET_RE.sub('-> <target>', line)
      line = _TRY_RANGE_RE.sub('[<target> .. <target>[', line)
      ret.append(line.rstrip() + '\n')
  return ret


_DISASSEMBLED_METHOD_QUOTA = 10
_SYMBOL_FULL_NAME_RE = re.compile(r'(.*?)#(.*?)\((.*?)\):? ?(.*)')


class _CachedApkDisassembler:
  def __init__(self):
    self._proguard_mapping_file_path_lookup = {}

  def AssignProguardMappingPath(
    self, apk_file_path, proguard_mapping_file_path
  ):
    self._proguard_mapping_file_path_lookup[apk_file_path] = (
      proguard_mapping_file_path
    )

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
        path_util.GetJavaExec(),
        '-cp',
        r8_path,
        'com.android.tools.r8.Disassemble',
        '--pg-map',
        mapping,
        tmp_file.name,
      ]
      r8_output = subprocess.check_output(cmd, encoding='utf-8')

    return r8_output

  @functools.lru_cache(None)
  def GetForApkAndSplit(self, apk_file_path, split_name):
    proguard_mapping_file_path = self._proguard_mapping_file_path_lookup[
      apk_file_path
    ]
    r8_output = None
    if split_name:
      logging.info('Creating disassembly for APK split: %s', split_name)
      with zip_util.UnzipToTemp(
        apk_file_path, f'splits/{split_name}-master.apk'
      ) as split_path:
        r8_output = self._DisassembleApk(proguard_mapping_file_path, split_path)
    elif apk_file_path.endswith('.apk'):
      logging.info('Creating disassembly for APK: %s', apk_file_path)
      r8_output = self._DisassembleApk(
        proguard_mapping_file_path, apk_file_path
      )
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
      bytecode = class_obj.FindMethodByteCode(
        class_name, method_name, param_types, return_type
      )
  return bytecode


def Disassemble(symbol, path_resolver, apk_disassembler_cache):
  logging.debug('Disassembling %s', symbol.full_name)
  container = symbol.container
  proguard_mapping_file_name = container.metadata.get(
    'proguard_mapping_file_name'
  )
  if proguard_mapping_file_name is None:
    raise Exception('Mapping file does not exist in container metadata.')

  proguard_mapping_file_path = path_resolver(proguard_mapping_file_name)
  apk_file_name = container.metadata['apk_file_name']
  apk_file_path = str(path_resolver(apk_file_name))
  split_name = container.metadata.get('apk_split_name')  # Can be None.
  apk_disassembler_cache.AssignProguardMappingPath(
    apk_file_path, proguard_mapping_file_path
  )
  class_obj_map = apk_disassembler_cache.GetForApkAndSplit(
    apk_file_path, split_name
  )
  if class_obj_map is None:
    return None
  logging.info('Looking up disassembly for %s', symbol.full_name)
  return _ExtractDisassemblyForMethod(class_obj_map, symbol.full_name)


def _AddUnifiedDiff(
  top_changed_symbols,
  before_path_resolver,
  after_path_resolver,
  normalize=False,
):
  after_apk_disassembler_cache = _CachedApkDisassembler()
  before_apk_disassembler_cache = _CachedApkDisassembler()
  for symbol in top_changed_symbols:
    after = None
    if symbol.after_symbol:
      after = Disassemble(
        symbol.after_symbol, after_path_resolver, after_apk_disassembler_cache
      )
    before = None
    if symbol.before_symbol:
      before = Disassemble(
        symbol.before_symbol,
        before_path_resolver,
        before_apk_disassembler_cache,
      )

    logging.info('Adding disassembly for: %s', symbol.full_name)
    if normalize:
      after = after and NormalizeLines(after)
      before = before and NormalizeLines(before)

    target_symbol = symbol.after_symbol or symbol.before_symbol
    target_symbol.disassembly = disassembly_util.CreateUnifiedDiff(
      symbol.full_name, before or [], after or []
    )


def _GetTopChangedSymbols(delta_size_info, changed_files=None):
  def filter_symbol(symbol):
    if symbol.name.startswith('*'):
      return False
    # Currently restricting the symbols to .dex.method symbols only.
    if not symbol.section_name.endswith('dex.method'):
      return False
    # Symbols which have changed under 10 bytes do not add much value.
    if abs(symbol.size_without_padding) < 10:
      return False
    # Giant symbols also rarely add value.
    if symbol.after_symbol and symbol.after_symbol.size > 10000:
      return False
    if symbol.before_symbol and symbol.before_symbol.size > 10000:
      return False
    return True

  candidates = delta_size_info.raw_symbols.Filter(filter_symbol)
  return disassembly_util.SampleSymbols(candidates, changed_files=changed_files)


def AddDisassembly(
  delta_size_info,
  before_path_resolver,
  after_path_resolver,
  normalize=False,
  changed_files=None,
):
  """Adds disassembly diffs to top changed dex symbols.

  Adds the unified diff on the "before" and "after" disassembly to the
  top |_DISASSEMBLED_METHOD_QUOTA| changed symbols.

  Args:
    delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
    before_path_resolver: Callable to compute paths for "before" artifacts.
    after_path_resolver: Callable to compute paths for "after" artifacts.
    normalize: Whether to normalize the disassembly.
  """
  logging.info('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(
    delta_size_info, changed_files=changed_files
  )
  logging.info(
    'Adding disassembly to top %d changed dex symbols',
    _DISASSEMBLED_METHOD_QUOTA,
  )
  _AddUnifiedDiff(
    top_changed_symbols,
    before_path_resolver,
    after_path_resolver,
    normalize=normalize,
  )


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('apk_path', type=str)
  parser.add_argument('mapping_file_path', type=str)
  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG)

  logging.info('Loading %s and %s...', args.apk_path, args.mapping_file_path)
  apk_disassembler_cache = _CachedApkDisassembler()
  apk_disassembler_cache.AssignProguardMappingPath(
    args.apk_path, args.mapping_file_path
  )
  class_obj_map = apk_disassembler_cache.GetForApkAndSplit(args.apk_path, None)
  variables = {'class_obj_map': class_obj_map}
  banner = []
  banner.append('=' * 80)
  banner.append('class_obj_map: {method: archive_util.DexClass obj}')
  code.InteractiveConsole(variables).interact('\n'.join(banner))


if __name__ == '__main__':
  main()
