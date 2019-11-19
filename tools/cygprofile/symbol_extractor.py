# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to get and manipulate symbols from a binary."""

import collections
import logging
import os
import re
import subprocess
import sys

import cygprofile_utils

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))

sys.path.insert(0, os.path.join(_SRC_PATH, 'build', 'android'))
from pylib.constants import host_paths

_MAX_WARNINGS_TO_PRINT = 200

SymbolInfo = collections.namedtuple('SymbolInfo', ('name', 'offset', 'size',
                                                   'section'))

# Unfortunate global variable :-/
_arch = 'arm'


def SetArchitecture(arch):
  """Set the architecture for binaries to be symbolized."""
  global _arch
  _arch = arch


# Regular expression to match lines printed by 'objdump -t -w'. An example of
# such line looks like this:
# 018db2de l     F .text  00000060              .hidden _ZN8SkBitmapC2ERKS_
#
# The regex intentionally allows matching more than valid inputs. This gives
# more protection against potentially incorrectly silently ignoring unmatched
# input lines. Instead a few assertions early in _FromObjdumpLine() check the
# validity of a few parts matched as groups.
_OBJDUMP_LINE_RE = re.compile(r'''
  # The offset of the function, as hex.
  (?P<offset>^[0-9a-f]+)

  # The space character.
  [ ]

  # The 7 groups of flag characters, one character each.
  (
    (?P<assert_scope>.)           # Global, local, unique local, etc.
    (?P<assert_weak_or_strong>.)
    (?P<assert_4spaces>.{4})      # Constructor, warning, indirect ref,
                                  # debugger symbol.
    (?P<symbol_type>.)            # Function, object, file or normal.
  )

  [ ]

  # The section name should start with ".text", can be ".text.foo". With LLD,
  # and especially LTO the traces of input sections are not preserved. Support
  # ".text.foo" for a little longer time because it is easy.
  (?P<section>.text[^0-9a-f]*)

  (?P<assert_tab> \s+)

  # The size of the symbol, as hex.
  (?P<size>[0-9a-f]+)

  # Normally separated out by 14 spaces, but some bits in ELF may theoretically
  # affect this length.
  (?P<assert_14spaces>[ ]+)

  # Hidden symbols should be treated as usual.
  (.hidden [ ])?

  # The symbol name.
  (?P<name>.*)

  $
  ''', re.VERBOSE)


def _FromObjdumpLine(line):
  """Create a SymbolInfo by parsing a properly formatted objdump output line.

  Args:
    line: line from objdump

  Returns:
    An instance of SymbolInfo if the line represents a symbol, None otherwise.
  """
  m = _OBJDUMP_LINE_RE.match(line)
  if not m:
    return None

  # A symbol can be (g)lobal, (l)ocal, or neither (a space). Per objdump's
  # manpage, "A symbol can be neither local or global for a variety of reasons".
  assert m.group('assert_scope') in set(['g', 'l', ' ']), line
  assert m.group('assert_weak_or_strong') in set(['w', ' ']), line
  assert m.group('assert_tab') == '\t', line
  assert m.group('assert_4spaces') == ' ' * 4, line
  assert m.group('assert_14spaces') == ' ' * 14, line
  name = m.group('name')
  offset = int(m.group('offset'), 16)

  # Output the label that contains the earliest offset. It is needed later for
  # translating offsets from the profile dumps.
  if name == cygprofile_utils.START_OF_TEXT_SYMBOL:
    return SymbolInfo(name=name, offset=offset, section='.text', size=0)

  # Check symbol type for validity and ignore some types.
  # From objdump manual page: The symbol is the name of a function (F) or a file
  # (f) or an object (O) or just a normal symbol (a space). The 'normal' symbols
  # seens so far has been function-local labels.
  symbol_type = m.group('symbol_type')
  if symbol_type == ' ':
    # Ignore local goto labels. Unfortunately, v8 builtins (like 'Builtins_.*')
    # are indistinguishable from labels of size 0 other than by name.
    return None
  # Guard against file symbols, since they are normally not seen in the
  # binaries we parse.
  assert symbol_type != 'f', line

  # Extract the size from the ELF field. This value sometimes does not reflect
  # the real size of the function. One reason for that is the '.size' directive
  # in the assembler. As a result, a few functions in .S files have the size 0.
  # They are not instrumented (yet), but maintaining their order in the
  # orderfile may be important in some cases.
  size = int(m.group('size'), 16)

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

  return SymbolInfo(name=name, offset=offset, section=m.group('section'),
                    size=size)


def _SymbolInfosFromStream(objdump_lines):
  """Parses the output of objdump, and get all the symbols from a binary.

  Args:
    objdump_lines: An iterable of lines

  Returns:
    A list of SymbolInfo.
  """
  name_to_offsets = collections.defaultdict(list)
  symbol_infos = []
  for line in objdump_lines:
    symbol_info = _FromObjdumpLine(line.rstrip('\n'))
    if symbol_info is not None:
      # On ARM the LLD linker inserts pseudo-functions (thunks) that allow
      # jumping distances farther than 16 MiB. Such thunks are known to often
      # reside on multiple offsets, they are not instrumented and hence they do
      # not reach the orderfiles. Exclude the thunk symbols from the warning.
      if not symbol_info.name.startswith('__ThumbV7PILongThunk_'):
        name_to_offsets[symbol_info.name].append(symbol_info.offset)
      symbol_infos.append(symbol_info)

  # Outlined functions are known to be repeated often, so ignore them in the
  # repeated symbol count.
  repeated_symbols = filter(lambda s: len(name_to_offsets[s]) > 1,
                            (k for k in name_to_offsets.keys()
                             if not k.startswith('OUTLINED_FUNCTION_')))
  if repeated_symbols:
    # Log the first 5 repeated offsets of the first 10 repeated symbols.
    logging.warning('%d symbols repeated with multiple offsets:\n %s',
                    len(repeated_symbols), '\n '.join(
                        '{} {}'.format(sym, ' '.join(
                            str(offset) for offset in name_to_offsets[sym][:5]))
                        for sym in repeated_symbols[:10]))

  return symbol_infos


def SymbolInfosFromBinary(binary_filename):
  """Runs objdump to get all the symbols from a binary.

  Args:
    binary_filename: path to the binary.

  Returns:
    A list of SymbolInfo from the binary.
  """
  command = (host_paths.ToolPath('objdump', _arch), '-t', '-w', binary_filename)
  p = subprocess.Popen(command, shell=False, stdout=subprocess.PIPE)
  try:
    result = _SymbolInfosFromStream(p.stdout)
    return result
  finally:
    p.stdout.close()
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
  command = (_NM_PATH, '-defined-only', filename)
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
  for infos in GroupSymbolInfosByName(symbol_infos).itervalues():
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
  cmd = [host_paths.ToolPath('c++filt', _arch)]
  process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  demangled_symbol, _ = process.communicate(mangled_symbol + '\n')
  return demangled_symbol
