# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

_OUTPUT = """\
{}:
0x06435d99: DW_TAG_compile_unit
              DW_AT_producer    ("")
              DW_AT_language    (DW_LANG_C_plus_plus_14)
              DW_AT_name        ("../../base/allocator/page_allocator.cc")
              DW_AT_stmt_list   (0x041db38d)
              DW_AT_comp_dir    (".")
              DW_AT_low_pc      (0x00000000)
              DW_AT_ranges      (0x00a6f848
                 [0x02a2000, 0x02a3000))
0x06435f01: DW_TAG_compile_unit
              DW_AT_producer    ("")
              DW_AT_language    (DW_LANG_C_plus_plus_14)
              DW_AT_name        ("/third_party/container/container.c")
              DW_AT_stmt_list   (0x0846bbf7)
              DW_AT_comp_dir    (".")
              DW_AT_low_pc      (0x00000000)
              DW_AT_ranges      (0x0150ea78
                 [0x028d800, 0x028da00)
                 [0x02a0000, 0x02a0020)
                 [0x0000001, 0x0000001))
"""


def main():
  paths = [p for p in sys.argv[1:] if not p.startswith('-')]
  sys.stdout.write(_OUTPUT.format(paths[0]))
  sys.stdout.write('\n')


if __name__ == '__main__':
  main()
