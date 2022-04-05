# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the native disassembly for symbols."""

import difflib
import logging
import os
import subprocess

import dex_disassembly
import models
import path_util
import readelf


def _DisassembleFunc(symbol, output_directory, elf_path):
  """Returns disassembly for the given symbol.

  Args:
    symbol: Must be a .text symbol and not a SymbolGroup.
    output_directory: Path to the output directory of the build.
    elf_path: Path to the executable containing the symbol. Required only
        when auto-detection fails.
  Returns:
    Array with the lines of disassembly for symbol.
  """
  assert not symbol.IsGroup()
  assert (symbol.address is not None
          and symbol.section_name == models.SECTION_TEXT)
  assert not symbol.IsDelta(), ('Cannot disasseble a Diff\'ed symbol. Try '
                                'passing .before_symbol or .after_symbol.')
  # Running objdump from an output directory means that objdump can
  # interleave source file lines in the disassembly.
  objdump_cwd = output_directory or '.'

  try:
    arch = readelf.ArchFromElf(elf_path)
  except Exception:
    logging.warning('llvm-readelf failed on: %s', elf_path)
    return None
  objdump_path = path_util.GetDisassembleObjDumpPath(arch)
  args = [
      os.path.relpath(objdump_path, objdump_cwd),
      '--disassemble',
      '--line-numbers',
      '--demangle',
      '--start-address=0x%x' % symbol.address,
      '--stop-address=0x%x' % symbol.end_address,
      os.path.relpath(elf_path, objdump_cwd),
  ]
  if output_directory:
    args.append('--source')

  try:
    return subprocess.check_output(args,
                                   encoding='utf-8').splitlines(keepends=True)
  except Exception:
    logging.warning('objdump failed on : %s', (' '.join(args)))
    return None


def _CreateUnifiedDiff(name, before, after):
  unified_diff = difflib.unified_diff(before,
                                      after,
                                      fromfile=name,
                                      tofile=name,
                                      n=10)
  # Strip new line characters as difflib.unified_diff adds extra newline
  # characters to the first few lines which we do not want.
  return ''.join(unified_diff)


def _AddUnifiedDiff(top_changed_symbols, before_directory, after_directory,
                    delta_size_info):
  # Counter used to skip over symbols where we couldn't find the disassembly.
  counter = 10
  before = None
  after = None
  for symbol in top_changed_symbols:
    logging.debug('Symbols to go: %d', counter)
    after_elf_name = symbol.after_symbol.container.metadata.get('elf_file_name')
    after_elf_path = os.path.join(after_directory, after_elf_name)
    if not os.path.exists(after_elf_path):
      logging.warning('%s does not exist.', after_elf_path)
      break
    after_out_directory = delta_size_info.after.build_config.get(
        'out_directory')
    after = _DisassembleFunc(symbol.after_symbol, after_out_directory,
                             after_elf_path)
    if after is None:
      continue
    if symbol.before_symbol:
      before_elf_name = symbol.before_symbol.container.metadata.get(
          'elf_file_name')
      before_elf_path = os.path.join(before_directory, before_elf_name)
      before_out_directory = delta_size_info.before.build_config.get(
          'out_directory')
      before = _DisassembleFunc(symbol.before_symbol, before_out_directory,
                                before_elf_path)
    else:
      before = None
    logging.info('Adding disassembly for symbol: %s', symbol.full_name)
    symbol.after_symbol.disassembly = _CreateUnifiedDiff(
        symbol.full_name, before or [], after)
    counter -= 1
    if counter == 0:
      break


def _GetTopChangedSymbols(delta_size_info):
  sorted_symbols = [
      symbol for symbol in delta_size_info.raw_symbols
      if symbol.section_name.endswith('.text') and symbol.after_symbol
  ]
  sorted_symbols.sort(key=lambda x: -abs(x.size))
  return sorted_symbols


def AddDisassembly(delta_size_info, before_directory, after_directory):
  """Adds disassembly diffs to top changed native symbols.

    Adds the unified diff on the "before" and "after" disassembly to the
    top 10 changed native symbols.

    Args:
      delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
      before_directory: Directory of the "before" APK.
      after_directory: Directory of the "after" APK.
  """
  logging.debug('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(delta_size_info)
  logging.debug('Adding disassembly to top 10 changed native symbols')
  _AddUnifiedDiff(top_changed_symbols, before_directory, after_directory,
                  delta_size_info)
