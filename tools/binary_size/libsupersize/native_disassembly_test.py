#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import native_disassembly


class NativeDisassemblyTest(unittest.TestCase):

  def testNormalizeLines(self):
    lines = [
        '  400540:\t55                   \tpush   %rbp\n',
        '  400541:\t48 89 e5             \tmov    %rsp,%rbp\n',
        '  400545:\te8 06 00 00 00       \tcallq  400550 <some_function>\n',
        '  40054a:\xeb 04                \tjmp    400550 <some_function>\n',
        ('  40054c:\t48 8d 3d 0d 0a 20 00 \tlea    0x200a0d(%rip),%rdi        '
         '# 600f60 <some_global>\n'),
        '  400550:\t5d                   \tpop    %rbp\n',
        '  400551:\tc3                   \tretq\n',
    ]
    expected = [
        'push   %rbp\n',
        'mov    %rsp,%rbp\n',
        'callq  <some_function>\n',
        'jmp    <some_function>\n',
        'lea    <target>(%rip),%rdi        # <some_global>\n',
        '<target>:\n',
        'pop    %rbp\n',
        'retq\n',
    ]
    actual = native_disassembly._NormalizeLines(lines)
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
