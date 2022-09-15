# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers that interact with the "readelf" tool."""

import re
import subprocess

import path_util


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
  machine = re.search('Machine:\s*(.+)', stdout).group(1)
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

  Returns:
    A dict of section_name->(start_address, size).
  """
  args = [path_util.GetReadElfPath(), '-S', '--wide', elf_path]
  stdout = subprocess.check_output(args, encoding='ascii')
  section_ranges = {}
  # Matches  [ 2] .hash HASH 00000000006681f0 0001f0 003154 04   A  3   0  8
  for match in re.finditer(r'\[[\s\d]+\] (\..*)$', stdout, re.MULTILINE):
    items = match.group(1).split()
    section_ranges[items[0]] = (int(items[2], 16), int(items[4], 16))
  return section_ranges


def CollectRelocationAddresses(elf_path):
  """Returns the list of addresses that are targets for relative relocations."""
  cmd = [path_util.GetReadElfPath(), '--relocs', elf_path]
  ret = subprocess.check_output(cmd, encoding='ascii').splitlines()
  # Grab first column from (sample output) '02de6d5c  00000017 R_ARM_RELATIVE'
  return [int(l.split(maxsplit=1)[0], 16) for l in ret if 'R_ARM_RELATIVE' in l]
