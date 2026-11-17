# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class to get the native disassembly for symbols."""

import contextlib
import itertools
import logging
import os
import re
import shlex
import subprocess

import dex_disassembly
import disassembly_util
import models
import path_util
import readelf


# E.g. "  400540:	55                    \tpush   %rbp"
_DISASSEMBLY_RE = re.compile(r'^\s*([0-9a-f]+):\s*(.*)', re.IGNORECASE)
# E.g. "callq  400450 <some_func>" or "je     40055b <frame_dummy+0x10>"
_HEX_ADDR_WITH_SYM_RE = re.compile(r'\b([0-9a-f]{4,16})\s+<([^>]+)>')
# E.g. "0x200aa3" or "401060"
_RAW_HEX_ADDR_RE = re.compile(r'\b(0x[0-9a-f]{6,16}|[0-9a-f]{6,16})\b')
# E.g. "02a1fd44 <android_webview::JsSandboxIsolate::ReadFileDescriptorOnThread>:"
_HEADER_SYMBOL_LINE_RE = re.compile(r'^[0-9a-f]+\s+<.*>:$', re.IGNORECASE)
# E.g. "55", "e8 06 00 00 00", "4b00 1a2b"
_HEX_TOKEN_RE = re.compile(
  r'[0-9a-fA-F]{2}|[0-9a-fA-F]{4}|[0-9a-fA-F]{8}', re.IGNORECASE
)
# E.g. "base::internal::IntToStringT(...) (.llvm.13055180170483094575)"
_LLVM_HASH_RE = re.compile(r'\(\.llvm\.[0-9a-f]+\)')
# E.g. "<my_func+0x404>" or "<my_func-16>"
_SYM_OFFSET_RE = re.compile(r'[\+\-](?:0x)?[0-9a-f]+\s*>', re.IGNORECASE)
# E.g. "@ imm = #0xac" or "@ imm = #-0x126"
_IMM_COMMENT_RE = re.compile(
  r'@\s*imm\s*=\s*#-?(?:0x[0-9a-f]+|[0-9]+|<target>)'
)
# E.g. "@ <target> <my_func+0x404>"
_TARGET_COMMENT_RE = re.compile(r'@\s*<target>.*')
# E.g. "[sp, #0x90]" or "[sp, #136]"
_SP_OFFSET_BRACKET_RE = re.compile(
  r'\[sp,\s*#-?(?:0x[0-9a-f]+|[0-9]+)\]', re.IGNORECASE
)
# E.g. "sp, #0xac" or "sp, sp, #0x10000"
_SP_OFFSET_NO_BRACKET_RE = re.compile(
  r'\bsp,\s*(?:sp,\s*)?#-?(?:0x[0-9a-f]+|[0-9]+)\b', re.IGNORECASE
)
# E.g. "r5, sp, #0x88"
_REG_SP_OFFSET_RE = re.compile(
  r'\b([a-z0-9]+),\s*sp,\s*#-?(?:0x[0-9a-f]+|[0-9]+)\b', re.IGNORECASE
)
# E.g. "-0x10(%rsp)" or "0x18(%rbp)"
_X86_RSP_OFFSET_RE = re.compile(
  r'-?(?:0x[0-9a-f]+|[0-9]+)\(%r[sb]p\)', re.IGNORECASE
)
# E.g. "[rsp + 0x18]" or "[rbp - 16]"
_X86_RSP_BRACKET_OFFSET_RE = re.compile(
  r'\[r[sb]p\s*[\+\-]\s*(?:0x[0-9a-f]+|[0-9]+)\]', re.IGNORECASE
)
# E.g. "sub $0x20,%rsp" or "add $0x10,%rbp"
_X86_RSP_SUB_ADD_RE = re.compile(
  r'(sub|add)\s+\$0x[0-9a-f]+,\s*%r[sb]p', re.IGNORECASE
)
# E.g. "[pc, #0x3f4]" or "[lr, #0xa8]"
_BASE_REG_OFFSET_RE = re.compile(
  r'\[(pc|lr),\s*#-?(?:0x[0-9a-f]+|[0-9]+)\]', re.IGNORECASE
)
# E.g. "x0", "w29"
_ARM64_REG_RE = re.compile(r'\b[xw]([0-9]|[12][0-9]|3[01])\b')
# E.g. "r0", "r12"
_ARM_REG_RE = re.compile(r'\br([0-9]|1[0-5])\b')
# E.g. "%r8", "%r15d"
_X86_REG_RE = re.compile(r'%r([89]|1[0-5])[dbw]?\b')


def _NormalizeLines(lines):
  # Pass 1: Collect target addresses
  target_addresses = set()
  for line in lines:
    m = _DISASSEMBLY_RE.match(line)
    if m:
      instr = m.group(2)
      for match in _HEX_ADDR_WITH_SYM_RE.finditer(instr):
        target_addresses.add(int(match.group(1), 16))
      for match in _RAW_HEX_ADDR_RE.finditer(instr):
        target_addresses.add(int(match.group(1), 16))

  # Pass 2: Normalize and insert labels
  ret = []
  for line in lines:
    m = _DISASSEMBLY_RE.match(line)
    if m:
      addr = int(m.group(1), 16)
      if addr in target_addresses:
        if not ret or ret[-1] != '<target>:\n':
          ret.append('<target>:\n')

      # Extract instruction opcode/operands stripping address & hex bytes
      line_no_addr = m.group(2)
      parts = line_no_addr.split('\t')
      instr_parts = []
      for part in parts:
        p = part.strip()
        if not p:
          continue
        tokens = p.split()
        if tokens and all(
          _HEX_TOKEN_RE.fullmatch(t) or (len(t) == 1 and ord(t) > 127)
          for t in tokens
        ):
          continue
        instr_parts.append(p)
      instr = ' '.join(instr_parts) if instr_parts else line_no_addr.strip()

      instr = _HEX_ADDR_WITH_SYM_RE.sub(r'<\2>', instr)
      instr = _RAW_HEX_ADDR_RE.sub('<target>', instr)

      # LLVM hash suffixes: (.llvm.12345) -> (.llvm)
      instr = _LLVM_HASH_RE.sub('(.llvm)', instr)

      # Remove relative byte offsets from symbol references:
      # <symbol+0x123> -> <symbol>
      instr = _SYM_OFFSET_RE.sub('>', instr)

      # Immediate comments from objdump: @ imm = #0xac -> @ imm = <imm>
      instr = _IMM_COMMENT_RE.sub('@ imm = <imm>', instr)

      # PC-relative comment annotations e.g. @ <target> ...
      instr = _TARGET_COMMENT_RE.sub('@ <target>', instr)

      # Stack pointer offsets (ARM, ARM64, x86)
      instr = _SP_OFFSET_BRACKET_RE.sub('[sp, #<offset>]', instr)
      instr = _SP_OFFSET_NO_BRACKET_RE.sub('sp, #<offset>', instr)
      instr = _REG_SP_OFFSET_RE.sub(r'\1, sp, #<offset>', instr)
      instr = _X86_RSP_OFFSET_RE.sub('<offset>(%rsp)', instr)
      instr = _X86_RSP_BRACKET_OFFSET_RE.sub('[rsp + <offset>]', instr)
      instr = _X86_RSP_SUB_ADD_RE.sub(r'\1 $<offset>, %rsp', instr)

      # Base register load offsets (PC / LR)
      instr = _BASE_REG_OFFSET_RE.sub(r'[\1, #<offset>]', instr)

      # Register normalization: ARM r0-r15, x0-x31, w0-w31
      instr = _ARM64_REG_RE.sub('xN', instr)
      instr = _ARM_REG_RE.sub('rN', instr)
      instr = _X86_REG_RE.sub('%rN', instr)

      ret.append(instr.rstrip() + '\n')
    else:
      ret.append(line.rstrip() + '\n')
  return ret


# Don't disassemble more than this many bytes to guard against giant functions.
_MAX_DISASSEMBLY_BYTES = 2 * 1024


@contextlib.contextmanager
def Disassemble(
  symbol, output_directory, elf_path, max_bytes=_MAX_DISASSEMBLY_BYTES
):
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
    proc = subprocess.Popen(
      args, stdout=subprocess.PIPE, encoding='utf-8', cwd=objdump_cwd
    )
  except Exception:
    logging.warning('objdump failed: %s  # cwd=%s', cmd_str, objdump_cwd)
    yield []
    return

  truncated_str = '' if symbol.end_address == end_address else ' (truncated)'
  try:
    # objdump can be slow for large symbols, so it's helpful to stream the
    # output when in supersize console.
    yield itertools.chain(
      (
        'Showing disassembly for %r\n' % symbol,
        'Captured via: %s%s\n' % (shlex.join(args), truncated_str),
        '\n',
      ),
      proc.stdout,
    )
  finally:
    proc.kill()


def _ResolveOutputDirectory(size_info, output_directory):
  if output_directory is None:
    rel_out_dir = size_info.build_config.get(models.BUILD_CONFIG_OUT_DIRECTORY)
    if rel_out_dir:
      candidate = path_util.FromToolsSrcRoot(rel_out_dir)
      if os.path.isdir(candidate):
        output_directory = candidate
  return output_directory


def _DisassembleSymbol(symbol, path_resolver, out_directory, normalize=True):
  """Disassembles |symbol| from its ELF binary and sets |symbol.disassembly|."""
  if not symbol:
    return
  elf_path = str(path_resolver(symbol.container.metadata['elf_file_name']))
  if not os.path.exists(elf_path):
    logging.warning('%s does not exist.', elf_path)
    return
  with Disassemble(symbol, out_directory, elf_path) as lines:
    disasm_lines = list(lines)
  if disasm_lines and normalize:
    disasm_lines = _NormalizeLines(disasm_lines)
  if disasm_lines:
    symbol.disassembly = ''.join(disasm_lines)


def _GetTopChangedSymbols(delta_size_info, changed_files=None):
  def filter_symbol(symbol):
    # Don't want "** Thunk".
    if symbol.name.startswith('*'):
      return False
    # "aggregate padding" symbols.
    if not symbol.address:
      return False
    # Currently restricting the symbols to .text symbols only.
    if not symbol.section_name.endswith('.text'):
      return False
    # Symbols which have changed under 10 bytes do not add much value.
    if abs(symbol.size_without_padding) < 10:
      return False
    # Giant symbols also rarely add value.
    max_size = disassembly_util.MAX_DISASSEMBLED_SYMBOL_SIZE
    if symbol.after_symbol and symbol.after_symbol.size > max_size:
      return False
    if symbol.before_symbol and symbol.before_symbol.size > max_size:
      return False
    return True

  candidates = delta_size_info.raw_symbols.Filter(filter_symbol)
  return disassembly_util.SampleSymbols(candidates, changed_files=changed_files)


def AddDisassembly(
  delta_size_info,
  before_path_resolver,
  after_path_resolver,
  normalize=True,
  changed_files=None,
  output_directory=None,
):
  """Adds disassembly to top changed native symbols.

  Adds the normalized "before" and "after" disassembly to the
  top 10 changed native symbols.

  Args:
    delta_size_info: DeltaSizeInfo Object we are adding disassembly to.
    before_path_resolver: Callable to compute paths for "before" artifacts.
    after_path_resolver: Callable to compute paths for "after" artifacts.
    normalize: Whether to normalize the disassembly.
    output_directory: Optional path to the "after" build output directory.
  """
  logging.debug('Computing top changed symbols')
  top_changed_symbols = _GetTopChangedSymbols(
    delta_size_info, changed_files=changed_files
  )
  logging.debug('Adding disassembly to top 10 changed native symbols')
  out_directory = _ResolveOutputDirectory(
    delta_size_info.after, output_directory
  )
  for symbol in top_changed_symbols:
    _DisassembleSymbol(
      symbol.after_symbol, after_path_resolver, out_directory, normalize
    )
    # The source tree will have changed due to building "after", so it's
    # better to not include source lines than to include incorrect ones.
    _DisassembleSymbol(
      symbol.before_symbol, before_path_resolver, None, normalize
    )


def AddSizeInfoDisassembly(size_info, path_resolver, output_directory=None):
  """Adds disassembly to top 10 largest .text symbols in |size_info|."""
  top_symbols = disassembly_util.GetTopSizeInfoSymbols(
    size_info,
    lambda s: (
      s.address > 0
      and s.section_name.endswith('.text')
      and bool(s.container.metadata.get('elf_file_name'))
    ),
  )
  out_directory = _ResolveOutputDirectory(size_info, output_directory)
  for symbol in top_symbols:
    _DisassembleSymbol(symbol, path_resolver, out_directory)
