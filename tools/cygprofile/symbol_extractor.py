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


START_OF_TEXT_SYMBOL = 'linker_script_start_of_text'

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))
_TOOL_PREFIX = os.path.join(_SRC_PATH, 'third_party', 'llvm-build',
                            'Release+Asserts', 'bin', 'llvm-')

_MAX_WARNINGS_TO_PRINT = 200

SymbolInfo = collections.namedtuple('SymbolInfo', ('name', 'offset', 'size',
                                                   'section'))


def _IsExpectedSectionForInstrumentedCode(section):
  # Using __attribute__((section("any_name"))) one can put a function in a
  # section "any_name". The LLD linker puts this section in the same executable
  # segment as the section '.text'. The linker cannot reorder functions across
  # sections, so these functions outside `.text` will produce warnings during
  # orderfile verification. It is possible to exclude from the orderfile the
  # symbols from non-.text sections, but it is not done yet (as of 2024-07).
  #
  # The instrumentation hook (in orderfile_instrumentation.cc) warns against
  # offsets outside of the range between `linker_script_start_of_text` and
  # `linker_script_end_of_text`.
  #
  # The sections in the list below should be in sync with the
  # `anchor_functions.lds`.
  return section in ['.text', 'malloc_hook']


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
    # Skip symbols defined in other native libraries assuming they are not
    # instrumented.
    if section == 'Undefined':
      assert scope != 'Local', name
      continue
    # Skip non-function symbols (global variables, file references).
    if not symbol_type in ['Function', 'GNU_IFunc']:
      continue
    # Executable code can be in a section with any name, not only in '.text'.
    # Unfortunately, code reordering needs adjustments for each custom section
    # name. Break early on encountering symbols in unexpected sections to get
    # notified about adjustments due.
    assert _IsExpectedSectionForInstrumentedCode(section), (
        f'Symbol {name} in unexpected section "{section}"')
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
  p = subprocess.Popen(command,
                       shell=False,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE,
                       text=True)
  try:
    result = _SymbolInfosFromLlvmNm(p.stdout)
    if not result:
      file_size = os.stat(filename).st_size
      logging.warning('No symbols for %s (size %d)', filename, file_size)
    return result
  finally:
    _, _ = p.communicate()
    if p.stdout:
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
