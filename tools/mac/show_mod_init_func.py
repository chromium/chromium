#!/usr/bin/env python3

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Prints the contents of the module initialization functions stored in sections
matching the flag S_MOD_INIT_FUNC_POINTERS or S_INIT_FUNC_OFFSETS of a Mach-O
image.

Usage:
  tools/mac/show_mod_init_func.py out/gn/Chromium\ Framework.unstripped

This is meant to be used on a Mach-O executable. If a dSYM is present, use
dump-static-initializers.py instead.
"""

import argparse
import os
import platform
import re
import subprocess
import sys


# From <mach-o/loader.h>
# Section flag with only function pointers for initializers.
S_MOD_INIT_FUNC_POINTERS = 0x9
# Section flag with only 32-bit offsets to initializers.
S_INIT_FUNC_OFFSETS = 0x16


def GetArchitecture(binary, xcode_path):
  """If the binary is a fat file with multiple architectures, return its
  architecture that matches the host. If such an architecture is not present in
  the fat file print an error and exit. If the binary is a thin file or a
  single-architecture fat file, return the single architecture."""
  if xcode_path:
    lipo_path = os.path.join(xcode_path, 'Contents', 'Developer', 'Toolchains',
                             'XcodeDefault.xctoolchain', 'usr', 'bin', 'lipo')
  else:
    lipo_path = 'lipo'

  architectures = subprocess.check_output([lipo_path, '-archs', binary],
                                          encoding='utf-8').strip().split(' ')
  if len(architectures) == 1:
    return architectures[0]

  host_arch = platform.machine()
  if host_arch in architectures:
    return host_arch

  raise Exception('Host architecture ' + host_arch +
                  ' not present in fat binary')


def GetTextBase(load_commands):
  """Returns the base address of the __TEXT segment."""
  return int(
      re.search('segname __TEXT\n.*vmaddr (0x[0-9a-f]+)', load_commands,
                re.MULTILINE).group(1), 16)


def ShowModuleInitializers(binary, xcode_path):
  """Gathers the module initializers for |binary| and symbolizes the addresses.
  """
  # Get the architecture to operate on.
  architecture = GetArchitecture(binary, xcode_path)

  initializers = GetModuleInitializers(binary, architecture, xcode_path)
  if not initializers:
    # atos will do work even if there are no addresses, so bail early.
    return
  symbols = SymbolizeAddresses(binary, architecture, initializers, xcode_path)

  print(binary)
  for initializer in zip(initializers, symbols):
    print('%s @ %s' % initializer)


def GetStaticInitializerSection(load_commands):
  """Returns the static initializer location based on the binary load commands.
  Static initializers are stored in sections with flag S_MOD_INIT_FUNC_POINTERS
  or S_INIT_FUNC_OFFSETS. Below are some expected names of the the (sectname,
  segname,flags) that ld64 and lld would use:
  - deployment target macOS < 10.15 or iOS 14:
    (__mod_init_func,__DATA,S_MOD_INIT_FUNC_POINTERS)
  - deployment target macOS >= 10.15 or iOS 14:
    (__mod_init_func,__DATA_CONST,S_MOD_INIT_FUNC_POINTERS)
  - ld64 with a deployment target macOS >= 12 or iOS >= 16 or lldb with
    `-fixup_chains`:
    (__init_offsets,__TEXT,S_INIT_FUNC_OFFSETS)"""
  matches = re.findall(
      r'sectname (.*)\n\s+segname (.*)\n(?:.|\n)*?flags (0x[0-9a-f]*)\n',
      load_commands, re.MULTILINE)
  sections = []
  for sectname, segname, flags in matches:
    flags = int(flags, 16)
    if flags in (S_MOD_INIT_FUNC_POINTERS, S_INIT_FUNC_OFFSETS):
      sections.append((sectname, segname, flags))
  return sections


def GetModuleInitializers(binary, architecture, xcode_path):
  """Parses the __DATA,__mod_init_func segment of |binary| and returns a list
  of string hexadecimal addresses of the module initializers.
  """
  if xcode_path:
    otool_path = os.path.join(xcode_path, 'Contents', 'Developer', 'Toolchains',
                              'XcodeDefault.xctoolchain', 'usr', 'bin', 'otool')
  else:
    otool_path = 'otool'

  load_commands = subprocess.check_output(
      [otool_path, '-l', '-arch', architecture, binary], encoding='utf-8')

  static_initializer_sections = GetStaticInitializerSection(load_commands)
  addresses = []
  for sectname, segname, flags in static_initializer_sections:
    # The -v flag will display the addresses in a usable form (as opposed to
    # just its on-disk little-endian byte representation).
    otool = [
        otool_path, '-arch', architecture, '-v', '-s', segname, sectname, binary
    ]
    lines = subprocess.check_output(otool, encoding='utf-8').splitlines()
    # Skip the first two header lines and then get the address of the
    # initializer in the second column. The first address is the address of the
    # initializer pointer.
    #   out/gn/Chromium Framework.unstripped:
    #   Contents of (__DATA,__mod_init_func) section
    #   0x0000000008761498 0x000000000385d120
    if flags == S_MOD_INIT_FUNC_POINTERS:
      sect_address = [line.split(' ')[1] for line in lines[2:]]
      addresses.extend(sect_address)
      continue

    # If otool adds a proper implementation for S_INIT_FUNC_OFFSETS the
    # sections below building `sect_address` can be removed. The logic to add
    # the __TEXT base address will remain.
    if architecture not in ('arm64', 'x86_64'):
      raise Exception(
          "Parsing otool's S_INIT_FUNC_OFFSETS output on architectures other "
          "than arm64 on x86_64 is unsupported.")

    # Trim the warning that otool doesn't understand S_INIT_FUNC_OFFSETS.
    lines = [i for i in lines if not i.startswith('Unknown section')]

    # From https://github.com/apple-oss-distributions/cctools/blob/cctools-973.0.1/otool/ofile_print.c#L9553
    if architecture == 'arm64':
      # For arm64 otool hex dumps as 4-byte words.  Since the offsets
      # in S_INIT_FUNC_OFFSETS arm64 are 32 bits simply trim the first column
      sect_address = [line.split('\t')[1].strip() for line in lines[2:]]
      sect_address = (' '.join(sect_address)).split(' ')

    if architecture == 'x86_64':
      # For x86_64 otool dumps as byte-oriented output. Here, trim the first
      # column and recreate each 32 bit address from the 8 bit groups.
      octets = [line.split('\t')[1].strip() for line in lines[2:]]
      octets = (' '.join(octets)).split(' ')
      sect_address = []
      for i in range(0, len(octets), 4):
        # Take four octets and interpret as little-endian.
        sect_address.append(''.join(octets[i:i + 4][::-1]))

    # S_INIT_FUNC_OFFSETS are __TEXT relative. Add the __TEXT base
    # address to each initializer offset.
    text_base = GetTextBase(load_commands)
    sect_address = [hex(int(x, 16) + text_base) for x in sect_address]
    addresses.extend(sect_address)
  return addresses


def SymbolizeAddresses(binary, architecture, addresses, xcode_path):
  """Given a |binary| and a list of |addresses|, symbolizes them using atos.
  """
  if xcode_path:
    atos_path = os.path.join(xcode_path, 'Contents', 'Developer', 'usr',
        'bin', 'atos')
  else:
    atos_path = 'atos'

  atos = [atos_path, '-arch', architecture, '-o', binary] + addresses
  lines = subprocess.check_output(atos, encoding='utf-8').splitlines()
  return lines


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--xcode-path',
      default=None,
      help='Optional custom path to xcode binaries. By default, commands such '
      'as `otool` will be run as `/usr/bin/otool` which only works '
      'if there is a system-wide install of Xcode.')
  parser.add_argument('filename', nargs=1)
  options = parser.parse_args(args)

  ShowModuleInitializers(options.filename[0], options.xcode_path)
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
