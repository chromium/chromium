# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for parsing Dalvik bytecode."""

import collections
import struct

# Dalvik Bytecode specs copied from first two column of table in:
#   https://source.android.com/docs/core/runtime/dalvik-bytecode#instructions
# with minor modification (truncating comments).
_DALVIK_BYTECODE_SPECS = """00 10x  nop
01 12x  move vA, vB
02 22x  move/from16 vAA, vBBBB
03 32x  move/16 vAAAA, vBBBB
04 12x  move-wide vA, vB
05 22x  move-wide/from16 vAA, vBBBB
06 32x  move-wide/16 vAAAA, vBBBB
07 12x  move-object vA, vB
08 22x  move-object/from16 vAA, vBBBB
09 32x  move-object/16 vAAAA, vBBBB
0a 11x  move-result vAA
0b 11x  move-result-wide vAA
0c 11x  move-result-object vAA
0d 11x  move-exception vAA
0e 10x  return-void
0f 11x  return vAA
10 11x  return-wide vAA
11 11x  return-object vAA
12 11n  const/4 vA, #+B
13 21s  const/16 vAA, #+BBBB
14 31i  const vAA, #+BBBBBBBB
15 21h  const/high16 vAA, #+BBBB0000
16 21s  const-wide/16 vAA, #+BBBB
17 31i  const-wide/32 vAA, #+BBBBBBBB
18 51l  const-wide vAA, #+BBBBBBBBBBBBBBBB
19 21h  const-wide/high16 vAA, #+BBBB000000000000
1a 21c  const-string vAA, string@BBBB
1b 31c  const-string/jumbo vAA, string@BBBBBBBB
1c 21c  const-class vAA, type@BBBB
1d 11x  monitor-enter vAA
1e 11x  monitor-exit vAA
1f 21c  check-cast vAA, type@BBBB
20 22c  instance-of vA, vB, type@CCCC
21 12x  array-length vA, vB
22 21c  new-instance vAA, type@BBBB
23 22c  new-array vA, vB, type@CCCC
24 35c  filled-new-array {vC, vD, vE, vF, vG}, type@BBBB
25 3rc  filled-new-array/range {vCCCC .. vNNNN}, type@BBBB
26 31t  fill-array-data vAA, +BBBBBBBB (with supplemental data...)
27 11x  throw vAA
28 10t  goto +AA
29 20t  goto/16 +AAAA
2a 30t  goto/32 +AAAAAAAA
2b 31t  packed-switch vAA, +BBBBBBBB (with supplemental data...)
2c 31t  sparse-switch vAA, +BBBBBBBB (with supplemental data...)
2d..31 23x  cmpkind vAA, vBB, vCC
2d: cmpl-float (lt bias)
2e: cmpg-float (gt bias)
2f: cmpl-double (lt bias)
30: cmpg-double (gt bias)
31: cmp-long
32..37 22t  if-test vA, vB, +CCCC
32: if-eq
33: if-ne
34: if-lt
35: if-ge
36: if-gt
37: if-le
38..3d 21t  if-testz vAA, +BBBB
38: if-eqz
39: if-nez
3a: if-ltz
3b: if-gez
3c: if-gtz
3d: if-lez
3e..43 10x  (unused)
44..51 23x  arrayop vAA, vBB, vCC
44: aget
45: aget-wide
46: aget-object
47: aget-boolean
48: aget-byte
49: aget-char
4a: aget-short
4b: aput
4c: aput-wide
4d: aput-object
4e: aput-boolean
4f: aput-byte
50: aput-char
51: aput-short
52..5f 22c  iinstanceop vA, vB, field@CCCC
52: iget
53: iget-wide
54: iget-object
55: iget-boolean
56: iget-byte
57: iget-char
58: iget-short
59: iput
5a: iput-wide
5b: iput-object
5c: iput-boolean
5d: iput-byte
5e: iput-char
5f: iput-short
60..6d 21c  sstaticop vAA, field@BBBB
60: sget
61: sget-wide
62: sget-object
63: sget-boolean
64: sget-byte
65: sget-char
66: sget-short
67: sput
68: sput-wide
69: sput-object
6a: sput-boolean
6b: sput-byte
6c: sput-char
6d: sput-short
6e..72 35c  invoke-kind {vC, vD, vE, vF, vG}, meth@BBBB
6e: invoke-virtual
6f: invoke-super
70: invoke-direct
71: invoke-static
72: invoke-interface
73 10x  (unused)
74..78 3rc  invoke-kind/range {vCCCC .. vNNNN}, meth@BBBB
74: invoke-virtual/range
75: invoke-super/range
76: invoke-direct/range
77: invoke-static/range
78: invoke-interface/range
79..7a 10x  (unused)
7b..8f 12x  unop vA, vB
7b: neg-int
7c: not-int
7d: neg-long
7e: not-long
7f: neg-float
80: neg-double
81: int-to-long
82: int-to-float
83: int-to-double
84: long-to-int
85: long-to-float
86: long-to-double
87: float-to-int
88: float-to-long
89: float-to-double
8a: double-to-int
8b: double-to-long
8c: double-to-float
8d: int-to-byte
8e: int-to-char
8f: int-to-short
90..af 23x  binop vAA, vBB, vCC
90: add-int
91: sub-int
92: mul-int
93: div-int
94: rem-int
95: and-int
96: or-int
97: xor-int
98: shl-int
99: shr-int
9a: ushr-int
9b: add-long
9c: sub-long
9d: mul-long
9e: div-long
9f: rem-long
a0: and-long
a1: or-long
a2: xor-long
a3: shl-long
a4: shr-long
a5: ushr-long
a6: add-float
a7: sub-float
a8: mul-float
a9: div-float
aa: rem-float
ab: add-double
ac: sub-double
ad: mul-double
ae: div-double
af: rem-double
b0..cf 12x  binop/2addr vA, vB
b0: add-int/2addr
b1: sub-int/2addr
b2: mul-int/2addr
b3: div-int/2addr
b4: rem-int/2addr
b5: and-int/2addr
b6: or-int/2addr
b7: xor-int/2addr
b8: shl-int/2addr
b9: shr-int/2addr
ba: ushr-int/2addr
bb: add-long/2addr
bc: sub-long/2addr
bd: mul-long/2addr
be: div-long/2addr
bf: rem-long/2addr
c0: and-long/2addr
c1: or-long/2addr
c2: xor-long/2addr
c3: shl-long/2addr
c4: shr-long/2addr
c5: ushr-long/2addr
c6: add-float/2addr
c7: sub-float/2addr
c8: mul-float/2addr
c9: div-float/2addr
ca: rem-float/2addr
cb: add-double/2addr
cc: sub-double/2addr
cd: mul-double/2addr
ce: div-double/2addr
cf: rem-double/2addr
d0..d7 22s  binop/lit16 vA, vB, #+CCCC
d0: add-int/lit16
d1: rsub-int (reverse subtract)
d2: mul-int/lit16
d3: div-int/lit16
d4: rem-int/lit16
d5: and-int/lit16
d6: or-int/lit16
d7: xor-int/lit16
d8..e2 22b  binop/lit8 vAA, vBB, #+CC
d8: add-int/lit8
d9: rsub-int/lit8
da: mul-int/lit8
db: div-int/lit8
dc: rem-int/lit8
dd: and-int/lit8
de: or-int/lit8
df: xor-int/lit8
e0: shl-int/lit8
e1: shr-int/lit8
e2: ushr-int/lit8
e3..f9 10x  (unused)
fa 45cc invoke-polymorphic {vC, vD, vE, vF, vG}, meth@BBBB, proto@HHHH
fb 4rcc invoke-polymorphic/range {vCCCC .. vNNNN}, meth@BBBB, proto@HHHH
fc 35c  invoke-custom {vC, vD, vE, vF, vG}, call_site@BBBB
fd 3rc  invoke-custom/range {vCCCC .. vNNNN}, call_site@BBBB
fe 21c  const-method-handle vAA, method_handle@BBBB
ff 21c  const-method-type vAA, proto@BBBB
"""

DalvikByteCode = collections.namedtuple('DalvikByteCode',
                                        'op,size,format,name,params')


def _ParseByteCodeSpecs():
  """Parses _DALVIK_BYTECODE_SPECS into DalvikByteCode array."""
  format_map = [None] * 256
  name_map = [None] * 256
  params_map = [None] * 256
  (op_lo, op_hi) = (None, None)
  for line in _DALVIK_BYTECODE_SPECS.splitlines():
    comment_pos = line.find(' (')
    if comment_pos >= 0:
      line = line[:comment_pos]
    assert len(line) >= 5
    if line[2] == ':':
      # Inside op range, e.g.: 'b0: add-int/2addr'.
      # ['b0', 'add-int/2addr'].
      toks = line.split(': ')
      assert len(toks) == 2
      op = int(toks[0], 16)
      assert op_lo <= op <= op_hi
      name_map[op] = toks[1]  # 'add-int/2addr'.
      if op == op_hi:
        op_lo = op_hi = None
    elif line[2:4] == '..':
      # Define op range, e.g.: 'b0..cf 12x  binop/2addr vA, vB'.
      # ['b0..cf', '12x', 'binop/2addr', 'vA, vB'].
      toks = line.split(maxsplit=3)
      # (0xb0, 0xcf).
      (op_lo, op_hi) = (int(t, 16) for t in toks[0].split('..'))
      for op in range(op_lo, op_hi + 1):
        format_map[op] = toks[1]  # '12x'.
      if len(toks) > 2:  # If not unused.
        for op in range(op_lo, op_hi + 1):
          params_map[op] = toks[3]  # 'vA, vB'.
    else:
      # Standalone op, e.g.: '15 21h  const/high16 vAA, #+BBBB0000'.
      # ['15', '21h', 'const/high16', 'vAA, #+BBBB0000'].
      toks = line.split(maxsplit=3)
      op = int(toks[0], 16)
      format_map[op] = toks[1]  # '21h'.
      if len(toks) > 2:  # If not unused.
        name_map[op] = toks[2]  # 'const/high16'.
        params_map[op] = toks[3] if len(toks) >= 4 else ''  # 'vAA, #+BBBB0000'.

  ret = []
  for op in range(256):
    size = int(format_map[op][0]) * 2  # '21h' -> 4.
    bc = DalvikByteCode(op, size, format_map[op], name_map[op], params_map[op])
    ret.append(bc)
  return ret


DALVIK_INSTRUCTIONS = _ParseByteCodeSpecs()


def Split(insns):
  """Splits Dalvik code into a series of instruction bytes.

  The minimalistic approach avoids wasted work. It's up to the caller to filter
  and/or disassemble emitted bytes. It is assumed that supplemental data (from
  31t instructions {fill-array-data, packed-switch, sparse-switch}) are found at
  the end of `insns`. These are detected and omitted.

  Args:
    insns: Even-length bytearray data containing valid Dalvik code.
  """
  pos_end = len(insns)
  assert pos_end % 2 == 0
  pos = 0
  while pos < pos_end:
    instr = DALVIK_INSTRUCTIONS[insns[pos]]
    size = instr.size
    chunk = insns[pos:pos + size]
    # Instructions with supplemental data contains relative offset to where
    # data starts, which indicates where code ends.
    if instr.format == '31t':
      offset = struct.unpack_from('<L', chunk, 2)[0]
      pos_end = min(pos_end, pos + offset * 2)
    yield chunk
    pos += size
  # Do not emit supplemental data.
