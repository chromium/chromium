#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper for converting Windows HRESULT defines to enums.xml entries.

It only works with HRESULTs defined using `_HRESULT_TYPEDEF_`, e.g.
  #define MF_E_SAMPLE_NOT_WRITABLE         _HRESULT_TYPEDEF_(0xC00D36E0L)
will be converted to
  <int value="-1072875808" label="MF_E_SAMPLE_NOT_WRITABLE"/>

Some Windows files may use different or no macros to define HRESULTs, e.g.
  #define DRM_E_FILEOPEN                 ((DRM_RESULT)0x8003006EL)
  #define MF_INDEX_SIZE_ERR              0x80700001
This script will not work in those cases, but is easy to be modified to work.

Usage:
tools/hresult_to_enum.py -i mferror.h -o mferror.xml
"""

import argparse
import re
import sys

_HRESULT_RE = re.compile(
    r'^#define\s+([0-9A-Z_]+)\s+.*_HRESULT_TYPEDEF_\((0x[0-9A-F]{8}).*')


def _HexToSignedInt(hex_str):
  """Converts a hex string to a signed integer string.

  Args:
    hex_str: A string representing a hex number.

  Returns:
    A string representing the converted signed integer.
  """
  int_val = int(hex_str, 16)
  if int_val & (1 << 31):
    int_val -= 1 << 32
  return str(int_val)


def _HresultToEnum(match):
  """Converts an HRESULT define to an enums.xml entry.
  """
  hresult = match.group(1)
  hex_str = match.group(2)
  int_str = _HexToSignedInt(hex_str)
  return f'<int value="{int_str}" label="{hresult}"/>'


def _ConvertAllHresultDefines(source):
  """Converts all HRESULT defines to enums.xml entries.
  """
  in_lines = source.splitlines()
  out_lines = []

  for in_line in in_lines:
    out_line, num_of_subs = _HRESULT_RE.subn(_HresultToEnum, in_line)
    assert num_of_subs <= 1
    if num_of_subs == 1:
      out_lines.append(out_line)

  return '\n'.join(out_lines)


def main():
  parser = argparse.ArgumentParser(
      description='Convert HEX HRESULT to signed integer.')
  parser.add_argument('-i',
                      '--input',
                      help='The input file containing HRESULT defines',
                      required=True)
  parser.add_argument('-o',
                      '--output',
                      help='The output file containing enums.xml entries',
                      required=True)
  args = parser.parse_args()

  with open(args.input, 'r') as f:
    new_source = _ConvertAllHresultDefines(f.read())
  with open(args.output, 'w', newline='\n') as f:
    f.write(new_source)


if __name__ == '__main__':
  sys.exit(main())
