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

  def testNormalizeLines_StackPointerOffsets(self):
    lines = [
        '  2a1fd44:\tb08b                \tsub\tsp, #0xac\n',
        '  2a1fd46:\te800 000c            \tldr\tr0, [sp, #0xc]\n',
        '  2a1fd4a:\te800 0090            \tldr\tr0, [sp, #0x90]\n',
        '  2a1fd4e:\te800 0088            \tadd\tr0, r0, [sp, #136]\n',
        '  2a1fd52:\t48 83 ec 20          \tsub    $0x20,%rsp\n',
        '  2a1fd56:\t48 8b 44 24 18       \tmov    0x18(%rsp),%rax\n',
    ]
    expected = [
        'sub sp, #<offset>\n',
        'ldr rN, [sp, #<offset>]\n',
        'ldr rN, [sp, #<offset>]\n',
        'add rN, rN, [sp, #<offset>]\n',
        'sub $<offset>, %rsp\n',
        'mov    <offset>(%rsp),%rax\n',
    ]
    actual = native_disassembly._NormalizeLines(lines)
    self.assertEqual(expected, actual)

  def testNormalizeLines_BaseRegistersAndBranches(self):
    lines = [
        ('  2a1fd44:\t4b00                \tldr\tr0, [pc, #0x3f4]        '
         '@ <target> <my_func+0x404>\n'),
        '  2a1fd48:\t4b01                \tldr\tr0, [lr, #0xa8]\n',
        ('  2a1fd4c:\tf7ff fffe            \tbl\t<target> <my_thunk>        '
         '@ imm = #0xac\n'),
        ('  2a1fd50:\tf7ff fffe            \tbl\t<target> <my_func+0x19a> '
         '@ imm = #-0x126\n'),
    ]
    expected = [
        'ldr rN, [pc, #<offset>]        @ <target>\n',
        'ldr rN, [lr, #<offset>]\n',
        'bl <target> <my_thunk>        @ imm = <imm>\n',
        'bl <target> <my_func> @ imm = <imm>\n',
    ]
    actual = native_disassembly._NormalizeLines(lines)
    self.assertEqual(expected, actual)

  def testNormalizeLines_HeadersCommentsAndHashes(self):
    lines = [
        'Showing disassembly for <libmonochrome.so>.text@2a1fd44\n',
        'Captured via: llvm-objdump --start-address=0x2a1fd44 ...\n',
        '/path/to/libmonochrome.so: file format elf32-littlearm\n',
        'Disassembly of section .text:\n',
        '02a1fd44 <my_func>:\n',
        '; ./../../path/to/file.cc:516\n',
        ';   if (lseek64(fd, offset, SEEK_SET) == -1) {\n',
        ('  2a1fd44:\tf7ff fffe            \tbl\t<target> '
         '<base::internal::IntToStringT(...) (.llvm.13055180170483094575)>\n'),
    ]
    expected = [
        'Showing disassembly for <libmonochrome.so>.text@2a1fd44\n',
        'Captured via: llvm-objdump --start-address=0x2a1fd44 ...\n',
        '/path/to/libmonochrome.so: file format elf32-littlearm\n',
        'Disassembly of section .text:\n',
        '02a1fd44 <my_func>:\n',
        '; ./../../path/to/file.cc:516\n',
        ';   if (lseek64(fd, offset, SEEK_SET) == -1) {\n',
        'bl <target> <base::internal::IntToStringT(...) (.llvm)>\n',
    ]
    actual = native_disassembly._NormalizeLines(lines)
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
