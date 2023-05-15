# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to get and manipulate symbols from a binary."""

import collections
import json
import logging
import os
import re
import subprocess
import sys

import cygprofile_utils

START_OF_TEXT_SYMBOL = 'linker_script_start_of_text'

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))
_TOOL_PREFIX = os.path.join(_SRC_PATH, 'third_party', 'llvm-build',
                            'Release+Asserts', 'bin', 'llvm-')

_MAX_WARNINGS_TO_PRINT = 200

SymbolInfo = collections.namedtuple('SymbolInfo', ('name', 'offset', 'size',
                                                   'section'))


def _SymbolInfosFromStream(input_file):
  """Parses the output of llvm-readelf, and gets all the symbols from a binary.

  Args:
    input_file: a .json file handle containing the readelf output.

  Returns:
    A list of SymbolInfo.
  """
  # Load the JSON output
  raw_symbols = json.load(input_file)
  # The file is structured as a list containing dictionaries, one per input
  # file.
  assert len(raw_symbols) == 1
  raw_symbols = raw_symbols[0]
  # Next have two sections: FileSummary and Symbols
  assert 'Symbols' in raw_symbols
  raw_symbols = raw_symbols['Symbols']

  name_to_offsets = collections.defaultdict(list)
  symbol_infos = []

  for symbol in raw_symbols:
    symbol = symbol['Symbol']
    name = symbol['Name']['Name']
    offset = symbol['Value']
    size = symbol['Size']
    section = symbol['Section']['Name']
    scope = symbol['Binding']['Name']
    # Output the label that contains the earliest offset. It is needed later for
    # translating offsets from the profile dumps.
    if name == START_OF_TEXT_SYMBOL:
      symbol_infos.append(
          SymbolInfo(name=name, offset=offset, section='.text', size=0))
      continue
    # Check symbol type for validity and ignore some types.
    symbol_type = symbol['Type']['Name']
    if symbol_type == 'None':
      # Ignore local goto labels. Unfortunately, v8 builtins (like
      # 'Builtins_.*') are indistinguishable from labels of size 0 other than
      # by name.
      continue
    if section != '.text':
      # Ignore anything that's outside the primary .text section
      continue
    assert symbol_type in ['Object', 'Function', 'File', 'GNU_IFunc']
    assert scope in ['Local', 'Global', 'Weak']
    # Forbid ARM mapping symbols and other unexpected symbol names, but allow $
    # characters in a non-initial position, which can appear as a component of a
    # mangled name, e.g. Clang can mangle a lambda function to:
    # 02cd61e0 l     F .text  000000c0 _ZZL11get_globalsvENK3$_1clEv
    # The equivalent objdump line from GCC is:
    # 0325c58c l     F .text  000000d0 _ZZL11get_globalsvENKUlvE_clEv
    #
    # Also disallow .internal and .protected symbols (as well as other flags),
    # those have not appeared in the binaries we parse. Rejecting these extra
    # prefixes is done by disallowing spaces in symbol names.
    assert re.match('^[a-zA-Z0-9_.][a-zA-Z0-9_.$]*$', name), name

    symbol_info = SymbolInfo(name=name,
                             offset=offset,
                             section=section,
                             size=size)
    # On ARM the LLD linker inserts pseudo-functions (thunks) that allow
    # jumping distances farther than 16 MiB. Such thunks are known to often
    # reside on multiple offsets, they are not instrumented and hence they do
    # not reach the orderfiles. Exclude the thunk symbols from the warning.
    if not symbol_info.name.startswith('__ThumbV7PILongThunk_'):
      name_to_offsets[symbol_info.name].append(symbol_info.offset)
    symbol_infos.append(symbol_info)

  # Outlined functions are known to be repeated often, so ignore them in the
  # repeated symbol count.
  repeated_symbols = list(
      filter(lambda s: len(name_to_offsets[s]) > 1,
             (k for k in name_to_offsets.keys()
              if not k.startswith('OUTLINED_FUNCTION_'))))
  if repeated_symbols:
    # Log the first 5 repeated offsets of the first 10 repeated symbols.
    logging.warning('%d symbols repeated with multiple offsets:\n %s',
                    len(repeated_symbols), '\n '.join(
                        '{} {}'.format(sym, ' '.join(
                            str(offset) for offset in name_to_offsets[sym][:5]))
                        for sym in repeated_symbols[:10]))

  return symbol_infos


def SymbolInfosFromBinary(binary_filename):
  """Runs llvm-readelf to get all the symbols from a binary.

  Args:
    binary_filename: path to the binary.

  Returns:
    A list of SymbolInfo from the binary.
  """
  command = [
      _TOOL_PREFIX + 'readelf', '--syms', '--elf-output-style=JSON',
      '--pretty-print', binary_filename
  ]
  try:
    p = subprocess.Popen(command,
                         stdout=subprocess.PIPE,
                         universal_newlines=True)
  except OSError as error:
    logging.error('Failed to execute the command: path=%s, binary_filename=%s',
                  command[0], binary_filename)
    raise error

  try:
    return _SymbolInfosFromStream(p.stdout)
  finally:
    p.wait()


_LLVM_NM_LINE_RE = re.compile(
    r'^[\-0-9a-f]{8,16}[ ](?P<symbol_type>.)[ ](?P<name>.*)$', re.VERBOSE)


def _SymbolInfosFromLlvmNm(lines):
  """Extracts all defined symbols names from llvm-nm output.

  Only defined (weak and regular) symbols are extracted.

  Args:
    lines: Iterable of lines.

  Returns:
    [str] A list of symbol names, can be empty.
  """
  symbol_names = []
  for line in lines:
    m = _LLVM_NM_LINE_RE.match(line)
    assert m is not None, line
    if m.group('symbol_type') not in ['t', 'T', 'w', 'W']:
      continue
    symbol_names.append(m.group('name'))
  return symbol_names


_NM_PATH = os.path.join(_SRC_PATH, 'third_party', 'llvm-build',
                        'Release+Asserts', 'bin', 'llvm-nm')


def CheckLlvmNmExists():
  assert os.path.exists(_NM_PATH), (
      'llvm-nm not found. Please run '
      '//tools/clang/scripts/update.py --package=objdump to install it.')


def SymbolNamesFromLlvmBitcodeFile(filename):
  """Extracts all defined symbols names from an LLVM bitcode file.

  Args:
    filename: (str) File to parse.

  Returns:
    [str] A list of symbol names, can be empty.
  """
  command = (_NM_PATH, '--defined-only', filename)
  p = subprocess.Popen(command, shell=False, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  try:
    result = _SymbolInfosFromLlvmNm(p.stdout)
    if not result:
      file_size = os.stat(filename).st_size
      logging.warning('No symbols for %s (size %d)', filename, file_size)
    return result
  finally:
    _, _ = p.communicate()
    p.stdout.close()
    assert p.wait() == 0


def GroupSymbolInfosByOffset(symbol_infos):
  """Create a dict {offset: [symbol_info1, ...], ...}.

  As several symbols can be at the same offset, this is a 1-to-many
  relationship.

  Args:
    symbol_infos: iterable of SymbolInfo instances

  Returns:
    a dict {offset: [symbol_info1, ...], ...}
  """
  offset_to_symbol_infos = collections.defaultdict(list)
  for symbol_info in symbol_infos:
    offset_to_symbol_infos[symbol_info.offset].append(symbol_info)
  return dict(offset_to_symbol_infos)


def GroupSymbolInfosByName(symbol_infos):
  """Create a dict {name: [symbol_info1, ...], ...}.

  A symbol can have several offsets, this is a 1-to-many relationship.

  Args:
    symbol_infos: iterable of SymbolInfo instances

  Returns:
    a dict {name: [symbol_info1, ...], ...}
  """
  name_to_symbol_infos = collections.defaultdict(list)
  for symbol_info in symbol_infos:
    name_to_symbol_infos[symbol_info.name].append(symbol_info)
  return dict(name_to_symbol_infos)


def CreateNameToSymbolInfo(symbol_infos):
  """Create a dict {name: symbol_info, ...}.

  Args:
    symbol_infos: iterable of SymbolInfo instances

  Returns:
    a dict {name: symbol_info, ...}
    If a symbol name corresponds to more than one symbol_info, the symbol_info
    with the lowest offset is chosen.
  """
  # TODO(lizeb,pasko): move the functionality in this method into
  # check_orderfile.
  symbol_infos_by_name = {}
  warnings = cygprofile_utils.WarningCollector(_MAX_WARNINGS_TO_PRINT)
  for infos in GroupSymbolInfosByName(symbol_infos).values():
    first_symbol_info = min(infos, key=lambda x: x.offset)
    symbol_infos_by_name[first_symbol_info.name] = first_symbol_info
    if len(infos) > 1:
      warnings.Write('Symbol %s appears at %d offsets: %s' %
                     (first_symbol_info.name,
                      len(infos),
                      ','.join([hex(x.offset) for x in infos])))
  warnings.WriteEnd('symbols at multiple offsets.')
  return symbol_infos_by_name


def DemangleSymbol(mangled_symbol):
  """Return the demangled form of mangled_symbol."""
  cmd = [_TOOL_PREFIX + 'cxxfilt', mangled_symbol]
  return subprocess.check_output(cmd, universal_newlines=True).rstrip()
