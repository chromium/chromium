# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers that interact with the "readelf" tool."""

import argparse
import logging
import os
import re
import subprocess

import archive_util
import models
import path_util

_RELATIVE_RELOC_NAMES = frozenset(
  (
    'R_386_RELATIVE',
    'R_X86_64_RELATIVE',
    'R_ARM_RELATIVE',
    'R_AARCH64_RELATIVE',
    'R_RISCV_RELATIVE',
  )
)
_RELOC_SECTION_RE = re.compile(r'^Relocation section .* contains (\d+) entries')


def BuildIdFromElf(elf_path):
  """Returns the Build ID for the given binary."""
  args = [path_util.GetReadElfPath(), '-n', elf_path]
  stdout = subprocess.check_output(args, encoding='ascii')
  match = re.search(r'Build ID: (\w+)', stdout)
  assert match, 'Build ID not found from running: ' + ' '.join(args)
  return match.group(1)


def ArchFromElf(elf_path):
  """Returns the GN architecture for the given binary."""
  args = [path_util.GetReadElfPath(), '-h', elf_path]
  stdout = subprocess.check_output(args, encoding='ascii')
  machine = re.search(r'Machine:\s*(.+)', stdout).group(1)
  if machine == 'Intel 80386':
    return 'x86'
  if machine == 'Advanced Micro Devices X86-64':
    return 'x64'
  if machine == 'ARM':
    return 'arm'
  if machine == 'AArch64':
    return 'arm64'
  return machine


def SectionInfoFromElf(elf_path):
  """Finds the address and size of all ELF sections

  Merges custom sections (those without a "." prefix) into their proceeding
  sections.

  Returns:
    A dict of section_name->(start_address, size).
  """
  args = [path_util.GetReadElfPath(), '-S', '--wide', elf_path]
  stdout = subprocess.check_output(args, encoding='ascii')
  section_ranges = {}
  # Matches  [ 2] .hash HASH 00000000006681f0 0001f0 003154 04   A  3   0  8
  prev_section_name = None
  for match in re.finditer(r'\[[\s\d]+\] (.*)$', stdout, re.MULTILINE):
    items = match.group(1).split()

    section_name = items[0]
    # The first line looks like:
    # [ 0]                     NULL  00000000 000000 000000 00      0   0  0
    if section_name == 'NULL':
      continue
    assert not section_name.startswith('.debug'), (
      'Should not section sizes of an unstripped binary.'
    )

    # Stop if we hit any partitions.
    if section_name.endswith('_partition'):
      break

    section_type = items[1]
    if section_type == 'NOBITS':
      # Ensure we don't count BSS as real size.
      assert section_name in models.BSS_SECTIONS, (
        'BSS_SECTIONS out of date: ' + section_name
      )

    section_range = (int(items[2], 16), int(items[4], 16))

    # E.g. Merge user-defined sections. e.g.: malloc_hook, protected_memory.
    if not section_name.startswith('.'):
      logging.info('Merged %s into %s', section_name, prev_section_name)
      archive_util.ExtendSectionRangeAdjacent(
        section_ranges, prev_section_name, section_range[0], section_range[1]
      )
    else:
      section_ranges[items[0]] = section_range
      prev_section_name = section_name

  return section_ranges


def CollectRelocationsFromElf(elf_path):
  """Returns (sorted_relative_addrs, total_reloc_count) for |elf_path|."""
  args = [path_util.GetReadElfPath(), '-r', elf_path]
  stdout = subprocess.check_output(args, encoding='utf-8', errors='replace')
  addresses = []
  total_count = 0
  section_kind = None
  for line in stdout.splitlines():
    stripped = line.strip()
    if not stripped:
      section_kind = None
      continue
    if stripped.startswith('Relocation section '):
      section_kind = None
      match = _RELOC_SECTION_RE.match(stripped)
      if match:
        total_count += int(match.group(1))
      continue
    if stripped.startswith('Index:'):
      section_kind = 'relr'
      continue
    if stripped.startswith('Offset'):
      section_kind = 'rel'
      continue
    if section_kind == 'relr':
      # Indexed rows look like "0000: <entry> <addr> [sym...]". Empty bitmap
      # entries ("0001: 00000001") emit no address and may precede the next
      # "0002:" index on the same line. Continuation rows start with "<addr>".
      parts = stripped.split()
      pos = 0
      while pos + 1 < len(parts) and parts[pos].endswith(':'):
        pos += 2
      if pos < len(parts):
        addresses.append(int(parts[pos], 16))
    elif section_kind == 'rel' and '_RELATIVE' in line:
      parts = stripped.split(None, 3)
      if len(parts) >= 3 and parts[2] in _RELATIVE_RELOC_NAMES:
        addresses.append(int(parts[0], 16))
  # Merge per-section sorted runs (e.g. .rela.dyn before .relr.dyn).
  addresses.sort()
  return addresses, total_count


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('elf_path', type=os.path.realpath)
  args = parser.parse_args()
  logging.basicConfig(
    level=logging.DEBUG,
    format='%(levelname).1s %(relativeCreated)6d %(message)s',
  )

  # Other functions in this file have test entrypoints in object_analyzer.py.
  section_ranges = SectionInfoFromElf(args.elf_path)
  for name, (addr, size) in section_ranges.items():
    print(f'{name:20} {addr:20x}\t{size}')


if __name__ == '__main__':
  main()
