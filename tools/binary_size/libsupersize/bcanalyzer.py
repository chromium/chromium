#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs bcanalyzer to extract data from LLVM Bitcode (BC) files.

IsBitcodeFile():
  Reads the magic header of a file to quickly decide whether it is a BC file.

ParseTag():
  Heuristically parses a single-line tag from bcanalyzer dump (exporeted for
  testing).

RunBcAnalyzerOnIntermediates():
  BulkForkAndCall() target: Given BC file [paths], runs (llvm-)bcanalyzer on
  each path, parses the output, extracts strings, and returns {path: [strings]}.

This file can also be run stand-alone in order to test out the logic on smaller
sample sizes.
"""

import argparse
import os
import re
import subprocess

import parallel
import path_util


# Upper bound on number of bytes per character in strings. 4-byte / 32-bit
# strings are rare and are likely confused with 32-bit int arrays. So by
# default, only accept up to 2-byte / 16-bit strings.
_CHAR_WIDTH_LIMIT = 2

_RE_SPLIT = re.compile(r'=(\d+)')
# <TYPE_BLOCK_ID> children tags that should not be counted as types.
# - <NUMENTRY> is meta data.
# - <STRUCT_NAME> with the following <STRUCT_NAMED> (or other tag) are counted
#   as a single type entry.
_NON_TYPE_TAGS = set(['NUMENTRY', 'STRUCT_NAME'])

# Use bit-fields for tag types: 1 => Opening tag, 2 => Closed tag.
OPENING_TAG = 1
CLOSING_TAG = 2
SELF_CLOSING_TAG = OPENING_TAG | CLOSING_TAG


def _IsOpeningTag(tag_type):
  return tag_type & 1


def _IsClosingTag(tag_type):
  return tag_type & 2


def IsBitcodeFile(path):
  try:
    with open(path, 'rb') as f:
      return f.read(4) == b'BC\xc0\xde'
  except IOError:
    return False


def ParseTag(line):
  """Heuristically parses a single-line tag from bcanalyzer dump.

  Since input data are machine-generated, so we only need "good enough" parsing
  logic that favors simplicity. For example, '</FOO/>' is accepted.

  Args:
    line: Stripped line that may have a single-line tag with trailing text.

  Returns:
    (tag_type, tag, attrib_pos) if successful, else (None) * 3. Details:
    tag_type: One of {OPENING_TAG, CLOSING_TAG, SELF_CLOSING_TAG}.
    tag: The tag name.
    attrib_pos: Position in |line| to start parsing attributes.
  """
  # <TYPE_BLOCK_ID NumWords=103 BlockCodeSize=4>
  #     ==> (OPENING_TAG, 'TYPE_BLOCK_ID', 14).
  # <ARRAY abbrevid=9 op0=1 op1=7/> Trailing text!
  #     ==> (SELF_CLOSING_TAG, 'ARRAY', 6).
  # </TYPE_BLOCK_ID>
  #     ==> (CLOSING_TAG, 'TYPE_BLOCK_ID', 15).

  # Assumes |line| is stripped, i.e., so no indent and no trailing new line.
  if len(line) < 2 or line[0] != '<':
    return (None, None, None)
  tag_type, pos = (CLOSING_TAG, 2) if line[1] == '/' else (OPENING_TAG, 1)
  for i in range(pos, len(line)):
    if not line[i].isalnum() and line[i] != '_':
      if i == pos or not line[i] in ' >/':
        break
      end = line.find('>', i)
      if end < 0:
        break
      if line[end - 1] == '/':
        return (SELF_CLOSING_TAG, line[pos:i], i)
      return (tag_type, line[pos:i], i)
  return (None, None, None)


def _ParseOpItems(line, pos):
  """Heuristically extracts op0=# op1=# ... values from a single-line tag."""
  # <SETTYPE abbrevid=4 op0=42/>
  #         ^ pos = 8
  #     ==> iter([42]).
  # <CSTRING abbrevid=11 op0=84 op1=101 op2=115 op3=116 op4=56 op5=97/>
  #         ^ pos = 8
  #     ==> iter([84, 101, 115, 116, 56, 97]).
  # <STRING abbrevid=9 op0=1 op1=0 op2=0 op3=1 op4=1 op5=0/>
  #        ^ pos = 7
  #     ==> iter([1, 0, 0, 1, 1, 0]).
  # <DATA op0=8412 op1=101 op2=1150 op3=116 op4=5200 op5=98 op6=0/>
  #      ^ pos = 5
  #     ==> iter([8412, 101, 1150, 116, 5200, 98, 0]).

  # In particular, skip 'abbrevid=#'.
  start = line.index(' op', pos)
  end = line.index('>', start)
  for t in _RE_SPLIT.finditer(line[start:end]):
    yield int(t.group(1))


# Emits uint16 values as a stream of 2 bytes (little-endian).
def _UnpackUint16ListToBytes(items):
  for item in items:
    yield item & 0xFF
    yield (item >> 8) & 0xFF


# Emits uint32 values as a stream of 4 bytes (little-endian).
def _UnpackUint32ListToBytes(items):
  for item in items:
    yield item & 0xFF
    yield (item >> 8) & 0xFF
    yield (item >> 16) & 0xFF
    yield (item >> 24) & 0xFF


class _BcIntArrayType:
  """The specs of an integer array type."""

  # Lookup table to map from width to an unpacker that splits ints into bytes.
  _UNPACKER_MAP = {
    1: iter,
    2: _UnpackUint16ListToBytes,
    4: _UnpackUint32ListToBytes
  }

  def __init__(self, length, width):
    # Number of elements in the array.
    self.length = length
    # Number of bytes per element.
    self.width = width

  def ParseOpItemsAsBytes(self, line, attrib_pos, add_null_at_end):
    """Reads op0=# op=# ... values and returns them as a list of bytes.

    Interprets each op0=# op1=# ... value as a |self.width|-byte integer, splits
    them into component bytes (little-endian), and returns the result as string.

    Args:
      line: Stripped line of single-line tag with op0=# op1=# ... data.
      attrib_pos: Position in |line| where attribute list starts.
      add_null_add_end: Whether to append |'\x00' * self.width|.
    """
    items = _ParseOpItems(line, attrib_pos)
    unpacker = _BcIntArrayType._UNPACKER_MAP[self.width]
    s = bytes(unpacker(items))
    if add_null_at_end:
      s += b'\x00' * self.width
    # Rather stringent check to ensure exact size match.
    assert len(s) == self.length * self.width
    return s


class _BcTypeInfo:
  """Stateful parser of <TYPE_BLOCK_ID>, specialized for integer arrays."""

  # <TYPE_BLOCK_ID NumWords=103 BlockCodeSize=4>
  #   <NUMENTRY op0=9/>                  # Type ids should be in [0, 8].
  #   <INTEGER op0=8/>                   # Type id = 0: int8.
  #   <POINTER abbrevid=4 op0=0 op1=0/>  # Type id = 1: Pointer to type id 0
  #                                      #    ==> int8*.
  #   <ARRAY abbrevid=9 op0=4 op1=0/>    # Type id = 2: Array with 4 elements
  #                                      # of type id 0 ==> int8[4]
  #   <STRUCT_NAME op0=115 op1=116 op2=114/>  # Joins next Tag.
  #   <STRUCT_NAMED abbrevid=8 op0=0 op1=1/>  # Type id = 3: Struct (unused).
  #   <FUNCTION abbrevid=5 op0=0 op1=12/>  # Type id = 4: Function (unused).
  #   <INTEGER op0=16/>                  # Type id = 5: int16.
  #   <POINTER abbrevid=4 op0=5 op1=0/>  # Type id = 6: Pointer to type id 5
  #                                      #    ==> int16*.
  #   <INTEGER op0=32/>                  # Type id = 7: int32.
  #   <ARRAY abbrevid=9 op0=5 op1=4/>    # Type id = 8: Array with 4 elements
  #                                      # of type id 5 ==> int16[4]
  # <TYPE_BLOCK_ID>

  def __init__(self):
    # Auto-incrementing current type id.
    self.cur_type_id = 0
    # Maps from type id (of an integer) to number of bits.
    self.int_types = {}
    # Maps from type id (of an integer array) to _BcIntArrayType.
    self.int_array_types = {}

  def Feed(self, line, tag, attrib_pos):
    """Parses a single-line tag and store integer and integer array types.

    Args:
      line: Stripped line of single-line tag with op0=# op1=# ... data.
      tag: The tag type in |line| (child tag of <TYPE_BLOCK_ID>).
      attrib_pos: Position in |line| where attribute list starts.
    """
    if tag in _NON_TYPE_TAGS:
      return
    if tag == 'INTEGER':
      num_bits = next(_ParseOpItems(line, attrib_pos))  # op0.
      self.int_types[self.cur_type_id] = num_bits
    elif tag == 'ARRAY':
      [size, item_type_id] = list(_ParseOpItems(line, attrib_pos))  # op0, op1.
      bits = self.int_types.get(item_type_id)
      if bits is not None:  # |bits| can be None for non-int arrays.
        self.int_array_types[self.cur_type_id] = _BcIntArrayType(
            size, bits // 8)
    self.cur_type_id += 1

  def GetArrayType(self, idx):
    return self.int_array_types.get(idx)


def _ParseBcAnalyzer(lines):
  """A generator to extract bytes() from bcanalyzer dump of a BC file."""

  # ...
  # <TYPE_BLOCK_ID NumWords=103 BlockCodeSize=4>
  #    ... (See above; parsed by _BcTypeInfo)
  # <TYPE_BLOCK_ID>
  # ...
  # <CONSTANTS_BLOCK NumWords=93 BlockCodeSize=4>
  #   <SETTYPE abbrevid=4 op0=1/>  # Current type id := 1 ==> int8*.
  #   <CE_INBOUNDS_GEP op0=3 op1=4 op2=1 op3=12 op4=57 op5=12 op6=57/>
  #   <SETTYPE abbrevid=4 op0=2/>  # Current type id := 2 ==> int8[4].
  #   <CSTRING abbrevid=11 op0=70 op1=111 op2=111/> record string = 'Foo'
  #   <STRING abbrevid=11 op0=70 op1=111 op2=111 op3=1/>  # {'F','o','o',1}.
  #   <SETTYPE abbrevid=6 op0=7/>    # Current type id := 7 ==> int32.
  #   <INTEGER abbrevid=5 op0=2000/> # Stores 1000.
  #   <INTEGER abbrevid=5 op0=2001/> # Stores -1000.
  #   <SETTYPE abbrevid=4 op0=8/>    # Current type id := 8 ==> int16[4].
  #   <NULL/>
  #   <DATA abbrevid=11 op0=8400 op1=10100 op2=11500 op3=0/>
  # </CONSTANTS_BLOCK>
  # ...

  # Notes:
  # - Only parse first <TYPE_BLOCK_ID> and first <CONSTANTS_BLOCK>.
  # - <CONSTANTS_BLOCK> is stateful: A "current type id" exists, and that's set
  #   by <SETTYPE>, with op0= referring to type id.
  #   - For array lengths one needs to refer to the corresponding <ARRAY>.
  # - Strings / arrays are in <CSTRING>, <STRING>, and <DATA>.
  #   - abbrevid=# is redundant (repeats tag type) and unused
  #   - Character data are stored in op0=# op1=# ..., one per character. These
  #     values should fit in the proper range, and can be fairly large.
  #   - <CSTRING> has implicit 0 at end.
  #   - Data lengths agree with the length in the matching <ARRAY> entry.
  #   - "record string" text is not very useful: It only appears if all
  #     characters are printable.
  # - Signed vs. unsigned types are undistinguished.
  #   - In <INTEGER>, the op0= value is stored as 2 * abs(x) + (signed ? 0 : 1).
  #   - In <ARRAY> of int, values are coerced to unsigned type.
  # - Strings and int arrays are undistinguished.
  #   - <CSTRING>: If an uint8 array happens to end with 0, then this gets used!
  # - Arrays (or integers) of all-0 appear as <NULL/>. Presumably this gets
  #   placed into .bss section.

  STATE_VOID = 0
  STATE_TYPE_BLOCK = 1
  STATE_CONST_BLOCK = 2
  state = STATE_VOID

  type_info = None
  consts_cur_type = None

  # State machine to parse the *first* <TYPE_BLOCK_ID> to initialize
  # |type_info|, then the *first* <CONSTANTS_BLOCK> to yield strings.
  for line in lines:
    line = line.lstrip()
    (tag_type, tag, attrib_pos) = ParseTag(line)
    if tag_type is None:
      continue
    if state == STATE_VOID:
      if _IsOpeningTag(tag_type):
        if tag == 'TYPE_BLOCK_ID':
          if type_info is None:
            state = STATE_TYPE_BLOCK
            type_info = _BcTypeInfo()
        elif tag == 'CONSTANTS_BLOCK':
          if type_info is not None:
            state = STATE_CONST_BLOCK

    elif state == STATE_TYPE_BLOCK:
      if _IsClosingTag(tag_type) and tag == 'TYPE_BLOCK_ID':
        state = STATE_VOID
      else:
        type_info.Feed(line, tag, attrib_pos)

    elif state == STATE_CONST_BLOCK:
      if _IsClosingTag(tag_type) and tag == 'CONSTANTS_BLOCK':
        # Skip remaining data, including subsequent <CONSTANTS_BLOCK>s.
        break
      if tag == 'SETTYPE':
        try:
          consts_cur_type_id = next(_ParseOpItems(line, attrib_pos))  # op0.
        except StopIteration:
          return
        consts_cur_type = type_info.GetArrayType(consts_cur_type_id)
      elif consts_cur_type and consts_cur_type.width <= _CHAR_WIDTH_LIMIT:
        if tag in ['CSTRING', 'STRING', 'DATA']:
          # Exclude 32-bit / 4-byte strings since they're rarely used, and are
          # likely confused with 32-bit int arrays.
          s = consts_cur_type.ParseOpItemsAsBytes(line, attrib_pos,
                                                  tag == 'CSTRING')
          yield (consts_cur_type, s)


class _BcAnalyzerRunner:
  """Helper to run bcanalyzer and extract output lines. """

  def __init__(self, output_directory):
    self._args = [
        path_util.GetBcAnalyzerPath(), '--dump', '--disable-histogram'
    ]
    self._output_directory = output_directory

  def RunOnFile(self, obj_file):
    output = subprocess.check_output(
        self._args + [obj_file], cwd=self._output_directory).decode('ascii')
    return output.splitlines()


# This is a target for BulkForkAndCall().
def RunBcAnalyzerOnIntermediates(target, output_directory):
  """Calls bcanalyzer and returns encoded map from path to strings.

  Args:
    target: A list of BC file paths.
  """
  assert isinstance(target, list)
  runner = _BcAnalyzerRunner(output_directory)
  strings_by_path = {}
  for t in target:
    strings_by_path[t] = [s for _, s in _ParseBcAnalyzer(runner.RunOnFile(t))]
  # Escape strings by repr() so there will be no special characters to interfere
  # parallel.EncodeDictOfLists() and decoding.
  return parallel.EncodeDictOfLists(strings_by_path, value_transform=repr)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-directory', default='.')
  parser.add_argument('--char-width-limit', type=int)
  parser.add_argument('objects', type=os.path.realpath, nargs='+')

  args = parser.parse_args()
  base_path = os.path.normpath(args.output_directory)
  runner = _BcAnalyzerRunner(args.output_directory)
  if args.char_width_limit is not None:
    global _CHAR_WIDTH_LIMIT
    _CHAR_WIDTH_LIMIT = args.char_width_limit

  for obj_path in args.objects:
    rel_path = os.path.relpath(obj_path, base_path)
    print('File: %s' % rel_path)
    for cur_type, s in _ParseBcAnalyzer(runner.RunOnFile(obj_path)):
      print('    char%d[%d]: %r' % (cur_type.width * 8, cur_type.length, s))
    print('')


if __name__ == '__main__':
  main()
