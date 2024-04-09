# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the native disassembly for symbols."""

import contextlib
import difflib
import itertools
import logging
import os
import shlex
import subprocess

import dex_disassembly
import models
import path_util
import readelf


# Don't disassemble more than this many bytes to guard against giant functions.
_MAX_DISASSEMBLY_BYTES = 2 * 1024


@contextlib.contextmanager
def Disassemble(symbol,
                output_directory,
                elf_path,
                max_bytes=_MAX_DISASSEMBLY_BYTES):
  """Yields disassembly for the given symbol.

  Args:
    symbol: Must be a .text symbol and not a SymbolGroup.
    output_directory: Path to the output directory of the build.
    elf_path: Path to the executable containing the symbol. Required only
        when auto-detection fails.
    max_bytes: Stop disassembling after this many bytes.
  Returns:
    Array with the lines of disassembly for symbol.
  """
  # Shouldn't happen.
  if symbol.size_without_padding < 1:
    logging.info('Skipping due to zero size: %r', symbol)
    yield []
    return

  # Running objdump from an output directory means that objdump can
  # interleave source file lines in the disassembly.
  objdump_cwd = output_directory or '.'

  try:
    arch = readelf.ArchFromElf(elf_path)
  except Exception:
    logging.warning('llvm-readelf failed on: %s', elf_path)
    yield []
    return
  objdump_path = path_util.GetDisassembleObjDumpPath(arch)
  # E.g. "** thunk" symbols tend to be very large.
  end_address = symbol.end_address
  if max_bytes is not None and max_bytes > 0:
    end_address = min(end_address, symbol.address + max_bytes)
  args = [
      os.path.relpath(objdump_path, objdump_cwd),
      '--disassemble',
      '--line-numbers',
      '--demangle',
      '--start-address=0x%x' % symbol.address,
      '--stop-address=0x%x' % end_address,
      os.path.relpath(elf_path, objdump_cwd),
  ]
  if output_directory:
    args.append('--source')

  cmd_str = shlex.join(args)
  logging.info('Disassembling symbol: %r', symbol)
  logging.info('Running: %s  # cwd=%s', cmd_str, objdump_cwd)
  try:
    proc = subprocess.Popen(args,
                            stdout=subprocess.PIPE,
                            encoding='utf-8',
                            cwd=objdump_cwd)
  except Exception:
    logging.warning('objdump failed: %s  # cwd=%s', cmd_str, objdump_cwd)
    yield []
    return

  truncated_str = '' if symbol.end_address == end_address else ' (truncated)'
  try:
    # objdump can be slow for large symbols, so it's helpful to stream the
    # output when in supersize console.
    yield itertools.chain(
        ('Showing disassembly for %r\n' % symbol, 'Captured via: %s%s\n' %
         (shlex.join(args), truncated_str), '\n'), proc.stdout)
  finally:
    proc.kill()


def _CreateUnifiedDiff(name, before, after):
  unified_diff = difflib.unified_diff(before,
                                      after,
                                      fromfile=name,
                                      tofile=name,
                                      n=10)
  # Strip new line characters as difflib.unified_diff adds extra newline
  # characters to the first few lines which we do not want.
  return ''.join(unified_diff)


def _ResolveElfPath(elf_path):
  if os.path.exists(elf_path):
    return elf_path

  # See if it was a partitioned library and the __combined.so file exists.
  if elf_path.endswith('_partition.so'):
    parent, filename = os.path.split(elf_path)
    filename = filename[:filename.index('_')] + '__combined.so'
    combined_elf_path = os.path.join(parent, filename)
  else:
    combined_elf_path = elf_path[:-3] + '__combined.so'

  if os.path.exists(combined_elf_path):
    return combined_elf_path
  logging.warning('%s does not exist (nor does %s).', elf_path,
                  combined_elf_path)
  return None


def _AddUnifiedDiff(top_changed_symbols, before_path_resolver,
                    after_path_resolver, delta_size_info):
  # Counter used to skip over symbols where we couldn't find the disassembly.
  counter = 10
  before = None
  after = None
  for symbol in top_changed_symbols:
    before_symbol = symbol.before_symbol
    after_symbol = symbol.after_symbol
    logging.debug('Symbols to go: %d', counter)
    elf_name = after_symbol.container.metadata['elf_file_name']
    elf_path = _ResolveElfPath(after_path_resolver(elf_name))
    if elf_path is None:
      # Do not continue trying symbols since we'll likely hit the same issue.
      break

    out_directory = delta_size_info.after.build_config.get('out_directory')
    if out_directory and not os.path.exists(out_directory):
      out_directory = None
    with Disassemble(after_symbol, out_directory, elf_path) as lines:
      if not lines:
        continue
      after = list(lines)

    before = None
    if before_symbol:
      elf_name = before_symbol.container.metadata['elf_file_name']
      elf_path = _ResolveElfPath(before_path_resolver(elf_name))
      if elf_path:
        # The source tree will have changed due to building "after", so it's
        # better to not include source lines than to include incorrect ones.
        out_directory = None
        with Disassemble(before_symbol, out_directory, elf_path) as lines:
          before = list(lines)

    logging.info('Creating unified diff')
    after_symbol.disassembly = _CreateUnifiedDiff(symbol.full_name, before
                                                  or [], after)
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
    # Currently restricting the symbols to .text symbols only.
    if not symbol.section_name.endswith('.text'):
      return False
    # Symbols which have changed under 10 bytes do not add much value.
    if abs(symbol.pss_without_padding) < 10:
      return False
    if not symbol.address:
      # "aggregate padding" symbols.
      return False
    return True

  return delta_size_info.raw_symbols.Filter(filter_symbol).Sorted()


def AddDisassembly(delta_size_info, before_path_resolver, after_path_resolver):
  """Adds disassembly diffs to top changed native symbols.

    Adds the unified diff on the "before" and "after" disassembly to the
    top 10 changed native symbols.

    Args:
      delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
      before_path_resolver: Callable to compute paths for "before" artifacts.
      after_path_resolver: Callable to compute paths for "after" artifacts.
  """
  logging.debug('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(delta_size_info)
  logging.debug('Adding disassembly to top 10 changed native symbols')
  _AddUnifiedDiff(top_changed_symbols, before_path_resolver,
                  after_path_resolver, delta_size_info)
