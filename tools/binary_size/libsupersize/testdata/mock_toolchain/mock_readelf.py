# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


_HEADERS = """ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              DYN (Shared object file)
  Machine:                           ARM
  Version:                           0x1
  Entry point address:               0x0
  Start of program headers:          52 (bytes into file)
  Start of section headers:          628588000 (bytes into file)
  Flags:                             0x5000200, Version5 EABI, soft-float ABI
  Size of this header:               52 (bytes)
  Size of program headers:           32 (bytes)
  Number of program headers:         9
  Size of section headers:           40 (bytes)
  Number of section headers:         40
  Section header string table index: 39
"""

_SECTIONS = """There are 40 section headers, starting at offset 0x25777de0:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .interp           PROGBITS        00000154 000154 000013 00   A  0   0  1
  [ 2] .note.gnu.build-id NOTE           00000168 000168 000024 00   A  0   0  4
  [ 3] .dynsym           DYNSYM          0000018c 00018c 001960 10   A  4   1  4
  [ 4] .dynstr           STRTAB          00001b0c 001b0c 000fb9 00   A  0   0  1
  [ 5] .hash             HASH            00002ad4 002ad4 000a7c 04   A  3   0  4
  [ 6] .gnu.version      VERSYM          00003558 003558 00032c 02   A  3   0  2
  [ 7] .gnu.version_d    VERDEF          00003888 003888 00001c 00   A  4   1  4
  [ 8] .gnu.version_r    VERNEED         000038a4 0038a4 000060 00   A  4   3  4
  [ 9] .rel.dyn          REL             00003904 003904 288498 08   A  3   0  4
  [10] .rel.plt          REL             0029fbec 29fbec 000b00 08   A  3   0  4
  [11] .plt              PROGBITS        002a06ec 2a06ec 001094 00  AX  0   0  4
  [12] .text             PROGBITS       0028d900 28d900 2250ba8 00  AX  0   0 64
  [13] .rodata           PROGBITS      0266e5f0 000084 5a72e4 00   A  0   0 256
  [14] .ARM.exidx        ARM_EXIDX      02bd3d10 2bd3d10 1771c8 08  AL 12   0  4
  [15] .ARM.extab        PROGBITS       02bd5858 2bd5858 02cd50 00   A  0   0  4
  [16] .data.rel.ro.local PROGBITS      02c176f0 2c166f0 0c0e08 00  WA  0   0 16
  [17] .data.rel.ro      PROGBITS       02cd8500 2cd8500 104108 00  WA  0   0 16
  [18] .init_array       INIT_ARRAY     02ddc608 2ddc608 000008 00  WA  0   0  4
  [19] .fini_array       FINI_ARRAY     02ddc6f4 2ddc6f4 000008 00  WA  0   0  4
  [20] .dynamic          DYNAMIC        02ddc6fc 2ddc6fc 000130 08  WA  4   0  4
  [21] .got              PROGBITS       02ddc834 2ddc834 00a7cc 00  WA  0   0  4
  [22] .data             PROGBITS       02de7000 2de7000 018d88 00  WA  0   0 32
  [23] .bss              NOBITS         02dffda0 2dffda0 13d7e8 00  WA  0   0 32
  [35] .note.gnu.gold-version NOTE     00000000 22700c98 00001c 00      0   0  4
  [36] .ARM.attributes  ARM_ATTRIBUTES 00000000 22700cb4 00003c 00      0   0  1
  [37] .symtab           SYMTAB    00000000 22700cf0 105ef20 10     38 901679  4
  [38] .strtab           STRTAB       00000000 234c4950 213a4fe 00      0   0  1
  [39] .shstrtab         STRTAB        00000000 257b46da 0001b4 00      0   0  1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)
"""

_NOTES = """
Displaying notes found at file offset 0x00000168 with length 0x00000024:
  Owner                 Data size\tDescription
  GNU                   0x00000014\tNT_GNU_BUILD_ID (unique build ID bitstring)
    Build ID: WhatAnAmazingBuildId

Displaying notes found at file offset 0x226c41e8 with length 0x0000001c:
  Owner                 Data size\tDescription
  GNU                   0x00000009\tNT_GNU_GOLD_VERSION (gold version)
"""

_OBJECT_OUTPUTS = {
    'obj/third_party/icu/icuuc/ucnv_ext.o': """\
There are 71 section headers, starting at offset 0x3114:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .strtab           STRTAB          00000000 0029ac 000765 00      0   0  1
  [ 2] .text             PROGBITS        00000000 000034 000000 00  AX  0   0  4
  [ 3] .text.ucnv_extIni PROGBITS        00000000 000034 0000c6 00  AX  0   0  2
  [ 4] .rel.text.ucnv_ex REL             00000000 0023f4 000010 08     70   3  4
  [ 5] .ARM.exidx.text.u ARM_EXIDX       00000000 0000fc 000008 00  AL  3   0  4
  [60] .rodata.str1.1    PROGBITS        00000000 000015 000015 01 AMS  0   0  1
  [56] .debug_str        PROGBITS        00000000 000c50 0003c5 01  MS  0   0  1
  [57] .debug_abbrev     PROGBITS        00000000 001015 0000a1 00      0   0  1
  [58] .debug_info       PROGBITS        00000000 0010b6 000151 00      0   0  1
  [59] .rel.debug_info   REL             00000000 002544 0001e8 08     70  58  4
  [60] .debug_ranges     PROGBITS        00000000 001207 0000b0 00      0   0  1
  [61] .rel.debug_ranges REL             00000000 00272c 000130 08     70  60  4
  [62] .debug_macinfo    PROGBITS        00000000 0012b7 000001 00      0   0  1
  [63] .comment          PROGBITS        00000000 0012b8 000024 01  MS  0   0  1
  [64] .note.GNU-stack   PROGBITS        00000000 0012dc 000000 00      0   0  1
  [65] .ARM.attributes   ARM_ATTRIBUTES  00000000 0012dc 00003c 00      0   0  1
  [66] .debug_frame      PROGBITS        00000000 001318 0001e4 00      0   0  4
  [67] .rel.debug_frame  REL             00000000 00285c 0000e0 08     70  66  4
  [68] .debug_line       PROGBITS        00000000 0014fc 000965 00      0   0  1
  [69] .rel.debug_line   REL             00000000 00293c 000070 08     70  68  4
  [70] .symtab           SYMTAB          00000000 001e64 000590 10      1  74  4
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)
""",
    'obj/third_party/WebKit.a': """\

File: obj/third_party/WebKit.a(PaintChunker.o)
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)

File: obj/third_party/WebKit.a(ContiguousContainer.o)
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)
""",
    'obj/base/base/page_allocator.o': """\
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .rodata.str1.1    PROGBITS        00000000 000015 000005 01 AMS  0   0  1
""",
    'obj/third_party/ffmpeg/libffmpeg_internal.a': """\

File: obj/third_party/ffmpeg/libffmpeg_internal.a(fft_float.o)
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .rodata.str1.1    PROGBITS        00000000 000015 000005 01 AMS  0   0  1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)

File: obj/third_party/ffmpeg/libffmpeg_internal.a(fft_fixed.o)
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)
""",
    '../../third_party/gvr-android-sdk/libgvr_shim_static_arm.a': """\

File: ../../third_party/gvr-android-sdk/libgvr_shim_static_arm.a(\
libcontroller_api_impl.a_controller_api_impl.o)
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)

File: ../../third_party/gvr-android-sdk/libgvr_shim_static_arm.a(\
libport_android_jni.a_jni_utils.o)
There are 68 section headers, starting at offset 0x5650:

Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings)
  I (info), L (link order), G (group), T (TLS), E (exclude), x (unknown)
  O (extra OS processing required) o (OS specific), p (processor specific)
""",
}

def _PrintHeader(path):
  sys.stdout.write('\n')
  sys.stdout.write('File: ' + path + '\n')


def _PrintOutput(path):
  payload = _OBJECT_OUTPUTS.get(os.path.normpath(path))
  assert payload, 'No mock_nm.py entry for: ' + path
  sys.stdout.write(payload)
  sys.stdout.write('\n')


def main():
  paths = [p for p in sys.argv[1:] if not p.startswith('-')]
  if paths[0].endswith('.o') or paths[0].endswith('.a'):
    if len(paths) > 1:
      for path in paths:
        _PrintHeader(path)
        _PrintOutput(path)
    else:
      _PrintOutput(paths[0])
  elif sys.argv[1] == '-h':
    sys.stdout.write(_HEADERS)
  elif sys.argv[1] == '-S':
    sys.stdout.write(_SECTIONS)
  elif sys.argv[1] == '-n':
    sys.stdout.write(_NOTES)
  else:
    assert False, 'Invalid args: %s' % sys.argv


if __name__ == '__main__':
  main()
