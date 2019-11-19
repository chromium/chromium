# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_OUTPUT = """\
{}:	file format ELF32-arm-little

Program Header:
    PHDR off    0x00000034 vaddr 0x00000034 paddr 0x00000034 align 2**2
         filesz 0x00000160 memsz 0x00000160 flags r--
  INTERP off    0x00000194 vaddr 0x00000194 paddr 0x00000194 align 2**0
         filesz 0x00000013 memsz 0x00000013 flags r--
    LOAD off    0x00000000 vaddr 0x00000000 paddr 0x00000000 align 2**12
         filesz 0x02e86540 memsz 0x02e86540 flags r-x
    LOAD off    0x02e86540 vaddr 0x02e87540 paddr 0x02e87540 align 2**12
         filesz 0x0020751c memsz 0x0020751c flags rw-
    LOAD off    0x0308da60 vaddr 0x0308fa60 paddr 0x0308fa60 align 2**12
         filesz 0x00027048 memsz 0x0056d5bc flags rw-
    LOAD off    0x030b5714 vaddr 0x0364d714 paddr 0x0364d714 align 2**12
         filesz 0x00000000 memsz 0x00001000 flags rw-
 DYNAMIC off    0x0308ce08 vaddr 0x0308de08 paddr 0x0308de08 align 2**2
         filesz 0x000000f0 memsz 0x000000f0 flags rw-
   RELRO off    0x02e86540 vaddr 0x02e87540 paddr 0x02e87540 align 2**0
         filesz 0x0020751c memsz 0x00207ac0 flags r--
   STACK off    0x00000000 vaddr 0x00000000 paddr 0x00000000 align 2**64
         filesz 0x00000000 memsz 0x00000000 flags rw-
    NOTE off    0x000001a8 vaddr 0x000001a8 paddr 0x000001a8 align 2**2
         filesz 0x000000d8 memsz 0x000000d8 flags r--
 UNKNOWN off    0x00000280 vaddr 0x00000280 paddr 0x00000280 align 2**2
         filesz 0x00002498 memsz 0x00002498 flags r--

Dynamic Section:
  NEEDED               libdl.so
  NEEDED               libm.so
  NEEDED               libandroid.so
  NEEDED               liblog.so
  NEEDED               libjnigraphics.so
  NEEDED               libc.so
  SONAME               libmonochrome.so
  FLAGS                0x00000008
  FLAGS_1              0x00000001
  ANDROID_REL          0x00021de8
  ANDROID_RELSZ        0x00030b95
  RELENT               0x00000008
  RELCOUNT             0x00060367
  JMPREL               0x00052980
  PLTRELSZ             0x00000c48
  PLTGOT               0x0308e42c
  PLTREL               0x00000011
  SYMTAB               0x00002718
  SYMENT               0x00000010
  STRTAB               0x000133a0
  STRSZ                0x0000ea48
  GNU_HASH             0x0000f2d0
  INIT_ARRAY           0x0308de00
  INIT_ARRAYSZ         0x00000008
  FINI_ARRAY           0x02e87540
  FINI_ARRAYSZ         0x00000008
  VERSYM               0x0000dbd8
  VERNEED              0x0000f270
  VERNEEDNUM           0x00000003
Version References:
  required from libdl.so:
    0x00050d63 0x00 03 LIBC
  required from libm.so:
    0x00050d63 0x00 04 LIBC
  required from libc.so:
    0x00050d63 0x00 02 LIBC
"""


def _PrintOutput(path):
  sys.stdout.write(_OUTPUT.format(path))
  sys.stdout.write('\n')


def main():
  paths = [p for p in sys.argv[1:] if not p.startswith('-')]
  _PrintOutput(paths[0])


if __name__ == '__main__':
  main()
