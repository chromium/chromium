#!/usr/bin/env python

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Prints the contents of the __DATA,__mod_init_func section of a Mach-O image.

Usage:
  tools/mac/show_mod_init_func.py out/gn/Chromium\ Framework.unstripped

This is meant to be used on a Mach-O executable. If a dSYM is present, use
dump-static-initializers.py instead.
"""

from __future__ import print_function

import optparse
import os
import subprocess
import sys


def ShowModuleInitializers(binary, xcode_path):
  """Gathers the module initializers for |binary| and symbolizes the addresses.
  """
  initializers = GetModuleInitializers(binary, xcode_path)
  if not initializers:
    # atos will do work even if there are no addresses, so bail early.
    return
  symbols = SymbolizeAddresses(binary, initializers, xcode_path)

  print(binary)
  for initializer in zip(initializers, symbols):
    print('%s @ %s' % initializer)


def GetModuleInitializers(binary, xcode_path):
  """Parses the __DATA,__mod_init_func segment of |binary| and returns a list
  of string hexadecimal addresses of the module initializers.
  """
  if xcode_path:
    otool_path = os.path.join(xcode_path, 'Contents', 'Developer',
        'Toolchains', 'XcodeDefault.xctoolchain', 'usr', 'bin', 'otool')
  else:
    otool_path = 'otool'
  # The -v flag will display the addresses in a usable form (as opposed to
  # just its on-disk little-endian byte representation).
  otool = [otool_path, '-v', '-s', '__DATA', '__mod_init_func', binary]
  lines = subprocess.check_output(otool).strip().split('\n')

  # Skip the first two header lines and then get the address of the
  # initializer in the second column. The first address is the address
  # of the initializer pointer.
  #   out/gn/Chromium Framework.unstripped:
  #   Contents of (__DATA,__mod_init_func) section
  #   0x0000000008761498 0x000000000385d120
  return [line.split(' ')[1] for line in lines[2:]]


def SymbolizeAddresses(binary, addresses, xcode_path):
  """Given a |binary| and a list of |addresses|, symbolizes them using atos.
  """
  if xcode_path:
    atos_path = os.path.join(xcode_path, 'Contents', 'Developer', 'usr',
        'bin', 'atos')
  else:
    atos_path = 'atos'

  atos = [atos_path, '-o', binary] + addresses
  lines = subprocess.check_output(atos).strip().split('\n')
  return lines


def Main():
  parser = optparse.OptionParser(usage='%prog filename')
  parser.add_option(
      '--xcode-path',
      default=None,
      help='Optional custom path to xcode binaries. By default, commands such '
      'as `otool` will be run as `/usr/bin/otool` which only works '
      'if there is a system-wide install of Xcode.')
  opts, args = parser.parse_args()
  if len(args) != 1:
    parser.error('missing binary filename')
    return 1

  ShowModuleInitializers(args[0], opts.xcode_path)
  return 0

if __name__ == '__main__':
  sys.exit(Main())
