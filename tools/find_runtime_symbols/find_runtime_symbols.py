#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Find symbols in a binary corresponding to given runtime virtual addresses.

Note that source file names are treated as symbols in this script while they
are actually not.
"""

from __future__ import print_function

import json
import logging
import os
import sys

from static_symbols import StaticSymbolsInFile


_BASE_PATH = os.path.dirname(os.path.abspath(__file__))
_TOOLS_LINUX_PATH = os.path.join(_BASE_PATH, os.pardir, 'linux')
sys.path.insert(0, _TOOLS_LINUX_PATH)


from procfs import ProcMaps  # pylint: disable=F0401

try:
  from collections import OrderedDict  # pylint: disable=E0611
except ImportError:
  _SIMPLEJSON_PATH = os.path.join(_BASE_PATH, os.pardir, os.pardir,
                                  'third_party')
  sys.path.insert(0, _SIMPLEJSON_PATH)
  from simplejson import OrderedDict


FUNCTION_SYMBOLS = 0
SOURCEFILE_SYMBOLS = 1
TYPEINFO_SYMBOLS = 2

_MAPS_FILENAME = 'maps'
_FILES_FILENAME = 'files.json'


class RuntimeSymbolsInProcess(object):
  def __init__(self):
    self._maps = None
    self._static_symbols_in_filse = {}

  def find_procedure(self, runtime_address):
    for vma in self._maps.iter(ProcMaps.executable):
      if vma.begin <= runtime_address < vma.end:
        static_symbols = self._static_symbols_in_filse.get(vma.name)
        if static_symbols:
          return static_symbols.find_procedure_by_runtime_address(
              runtime_address, vma)
        else:
          return None
    return None

  def find_sourcefile(self, runtime_address):
    for vma in self._maps.iter(ProcMaps.executable):
      if vma.begin <= runtime_address < vma.end:
        static_symbols = self._static_symbols_in_filse.get(vma.name)
        if static_symbols:
          return static_symbols.find_sourcefile_by_runtime_address(
              runtime_address, vma)
        else:
          return None
    return None

  def find_typeinfo(self, runtime_address):
    for vma in self._maps.iter(ProcMaps.constants):
      if vma.begin <= runtime_address < vma.end:
        static_symbols = self._static_symbols_in_filse.get(vma.name)
        if static_symbols:
          return static_symbols.find_typeinfo_by_runtime_address(
              runtime_address, vma)
        else:
          return None
    return None

  @staticmethod
  def load(prepared_data_dir):
    symbols_in_process = RuntimeSymbolsInProcess()

    with open(os.path.join(prepared_data_dir, _MAPS_FILENAME), mode='r') as f:
      symbols_in_process._maps = ProcMaps.load_file(f)
    with open(os.path.join(prepared_data_dir, _FILES_FILENAME), mode='r') as f:
      files = json.load(f)

    # pylint: disable=W0212
    for vma in symbols_in_process._maps.iter(ProcMaps.executable_and_constants):
      file_entry = files.get(vma.name)
      if not file_entry:
        continue

      static_symbols = StaticSymbolsInFile(vma.name)

      nm_entry = file_entry.get('nm')
      if nm_entry and nm_entry['format'] == 'bsd':
        with open(os.path.join(prepared_data_dir, nm_entry['file']), 'r') as f:
          static_symbols.load_nm_bsd(f, nm_entry['mangled'])

      readelf_entry = file_entry.get('readelf-e')
      if readelf_entry:
        with open(os.path.join(prepared_data_dir, readelf_entry['file']),
                  'r') as f:
          static_symbols.load_readelf_ew(f)

      decodedline_file_entry = file_entry.get('readelf-debug-decodedline-file')
      if decodedline_file_entry:
        with open(os.path.join(prepared_data_dir,
                               decodedline_file_entry['file']), 'r') as f:
          static_symbols.load_readelf_debug_decodedline_file(f)

      symbols_in_process._static_symbols_in_filse[vma.name] = static_symbols

    return symbols_in_process


def _find_runtime_function_symbols(symbols_in_process, addresses):
  result = OrderedDict()
  for address in addresses:
    if isinstance(address, basestring):
      address = int(address, 16)
    found = symbols_in_process.find_procedure(address)
    if found:
      result[address] = found.name
    else:
      result[address] = '0x%016x' % address
  return result


def _find_runtime_sourcefile_symbols(symbols_in_process, addresses):
  result = OrderedDict()
  for address in addresses:
    if isinstance(address, basestring):
      address = int(address, 16)
    found = symbols_in_process.find_sourcefile(address)
    if found:
      result[address] = found
    else:
      result[address] = ''
  return result


def _find_runtime_typeinfo_symbols(symbols_in_process, addresses):
  result = OrderedDict()
  for address in addresses:
    if isinstance(address, basestring):
      address = int(address, 16)
    if address == 0:
      result[address] = 'no typeinfo'
    else:
      found = symbols_in_process.find_typeinfo(address)
      if found:
        if found.startswith('typeinfo for '):
          result[address] = found[13:]
        else:
          result[address] = found
      else:
        result[address] = '0x%016x' % address
  return result


_INTERNAL_FINDERS = {
    FUNCTION_SYMBOLS: _find_runtime_function_symbols,
    SOURCEFILE_SYMBOLS: _find_runtime_sourcefile_symbols,
    TYPEINFO_SYMBOLS: _find_runtime_typeinfo_symbols,
    }


def find_runtime_symbols(symbol_type, symbols_in_process, addresses):
  return _INTERNAL_FINDERS[symbol_type](symbols_in_process, addresses)


def main():
  # FIX: Accept only .pre data
  if len(sys.argv) < 2:
    sys.stderr.write("""Usage:
%s /path/to/prepared_data_dir/ < addresses.txt
""" % sys.argv[0])
    return 1

  log = logging.getLogger('find_runtime_symbols')
  log.setLevel(logging.WARN)
  handler = logging.StreamHandler()
  handler.setLevel(logging.WARN)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  log.addHandler(handler)

  prepared_data_dir = sys.argv[1]
  if not os.path.exists(prepared_data_dir):
    log.warn("Nothing found: %s" % prepared_data_dir)
    return 1
  if not os.path.isdir(prepared_data_dir):
    log.warn("Not a directory: %s" % prepared_data_dir)
    return 1

  symbols_in_process = RuntimeSymbolsInProcess.load(prepared_data_dir)
  symbols_dict = find_runtime_symbols(FUNCTION_SYMBOLS,
                                      symbols_in_process,
                                      sys.stdin)
  for address, symbol in symbols_dict.iteritems():
    if symbol:
      print('%016x %s' % (address, symbol))
    else:
      print('%016x' % address)

  return 0


if __name__ == '__main__':
  sys.exit(main())
