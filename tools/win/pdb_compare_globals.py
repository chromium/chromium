# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script uses ShowGlobals.exe to compare two PDBs to see what interesting
globals are present in one but not the other. You can either pass in a .pdb file
or you can pass in a .txt file that contains the results of calling ShowGlobals.
This helps when investigating size regressions. Often code-size regressions are
associated with global variable changes, and those global variables can be
easier to track and investigate than the code.

Typical output from ShowGlobals.exe is lines like these:

  #Dups   DupSize   Size  Section Symbol name     PDB name

  0       0       122784  2       kBrotliDictionary       chrome.dll.pdb
  1       1824    0       0       LcidToLocaleNameTable   chrome.dll.pdb
"""

from __future__ import print_function

import os
import subprocess
import sys


def LoadSymbols(pdb_name):
  result = {}
  extension = os.path.splitext(pdb_name)[1].lower()
  if extension in ['.pdb']:
    command = 'ShowGlobals.exe "%s"' % pdb_name
    lines = subprocess.check_output(command).splitlines()
  elif extension in ['.txt']:
    lines = open(pdb_name).readlines()
  else:
    print('Unrecognized extension in %s' % pdb_name)
    return result
  for line in lines:
    parts = line.split('\t')
    if len(parts) >= 5 and not line.startswith('#'):
      # Put the first four columns (the numerical data associated with a symbol)
      # into a dictionary indexed by the fifth column, which is the symbol name.
      symbol_name = parts[4]
      result[symbol_name] = parts[:4]
  return result


def ShowExtras(symbols_A, symbols_B, name_A, name_B):
  print('Symbols that are in %s but not in %s' % (name_A, name_B))
  for key in symbols_A:
    if not key in symbols_B:
      # Print all the numerical data, followed by the symbol name, separated by
      # tabs.
      print('\t'.join(symbols_A[key] + [key]))
  print()


def ShowDifferences(symbols_A, symbols_B, name_A, name_B):
  print('Symbols that are changed from %s to %s' % (name_A, name_B))
  for key in symbols_A:
    if key in symbols_B:
      value_a = symbols_A[key]
      value_b = symbols_B[key]
      if value_a != value_b:
        # Print the symbol name and then the two versions of the numerical data,
        # indented.
        print('%s changed from/to:' % key)
        print('\t' + '\t'.join(value_a))
        print('\t' + '\t'.join(value_b))
  print()


def main():
  symbols_1 = LoadSymbols(sys.argv[1])
  symbols_2 = LoadSymbols(sys.argv[2])

  if len(symbols_1) == 0:
    print('No data found in %s - fastlink?' % sys.argv[1])
    return
  if len(symbols_2) == 0:
    print('No data found in %s - fastlink?' % sys.argv[2])
    return

  print('%d interesting globals in %s, %d interesting globals in %s' %
        (len(symbols_1), sys.argv[1], len(symbols_2), sys.argv[2]))

  ShowExtras(symbols_1, symbols_2, sys.argv[1], sys.argv[2])
  ShowExtras(symbols_2, symbols_1, sys.argv[2], sys.argv[1])
  ShowDifferences(symbols_1, symbols_2, sys.argv[1], sys.argv[2])


if __name__ == '__main__':
  sys.exit(main())
