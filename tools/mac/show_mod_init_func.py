#!/usr/bin/env python

# Copyright 2016 The Chromium Authors. All rights reserved.
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
import subprocess
import sys


def ShowModuleInitializers(binary):
  """Gathers the module initializers for |binary| and symbolizes the addresses.
  """
  initializers = GetModuleInitializers(binary)
  if not initializers:
    # atos will do work even if there are no addresses, so bail early.
    return
  symbols = SymbolizeAddresses(binary, initializers)

  print(binary)
  for initializer in zip(initializers, symbols):
    print('%s @ %s' % initializer)


def GetModuleInitializers(binary):
  """Parses the __DATA,__mod_init_func segment of |binary| and returns a list
  of string hexadecimal addresses of the module initializers.
  """
  # The -v flag will display the addresses in a usable form (as opposed to
  # just its on-disk little-endian byte representation).
  otool = ['otool', '-v', '-s', '__DATA', '__mod_init_func', binary]
  lines = subprocess.check_output(otool).strip().split('\n')

  # Skip the first two header lines and then get the address of the
  # initializer in the second column. The first address is the address
  # of the initializer pointer.
  #   out/gn/Chromium Framework.unstripped:
  #   Contents of (__DATA,__mod_init_func) section
  #   0x0000000008761498 0x000000000385d120
  return [line.split(' ')[1] for line in lines[2:]]


def SymbolizeAddresses(binary, addresses):
  """Given a |binary| and a list of |addresses|, symbolizes them using atos.
  """
  atos = ['xcrun', 'atos', '-o', binary] + addresses
  lines = subprocess.check_output(atos).strip().split('\n')
  return lines


def Main():
  parser = optparse.OptionParser(usage='%prog filename')
  opts, args = parser.parse_args()
  if len(args) != 1:
    parser.error('missing binary filename')
    return 1

  ShowModuleInitializers(args[0])
  return 0

if __name__ == '__main__':
  sys.exit(Main())
