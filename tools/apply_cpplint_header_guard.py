#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Applies cpplint build/header_guard recommendations.

Reads cpplint build/header_guard recommendations from stdin and applies them.

Run cpplint for a single header:
cpplint.py --filter=-,+build/header_guard foo.h 2>&1 | grep build/header_guard

Run cpplint for all headers in dir foo in parallel:
find foo -name '*.h' | \
    xargs parallel cpplint.py --filter=-,+build/header_guard -- 2>&1 | \
    grep build/header_guard
"""

import sys

IFNDEF_MSG = '  #ifndef header guard has wrong style, please use'
ENDIF_MSG_START = '  #endif line should be "'
ENDIF_MSG_END = '"  [build/header_guard] [5]'
NO_GUARD_MSG = '  No #ifndef header guard found, suggested CPP variable is'


def process_cpplint_recommendations(cpplint_data):
  root = sys.argv[1] if len(sys.argv) > 1 else ''
  root = "_".join(root.upper().strip(r'[/]+').split('/'))+"_"
  for entry in cpplint_data:
    entry = entry.split(':')
    # The length of the entry may be less than 3,
    # e.g the last line of text 'Total errors found: xx'.
    if len(entry) < 3:
      continue
    header = entry[0]
    line = entry[1]
    index = int(line) - 1
    msg = entry[2].rstrip()
    if msg == IFNDEF_MSG:
      assert len(entry) == 4

      with open(header, 'rb') as f:
        content = f.readlines()

      if not content[index + 1].startswith(b'#define '):
        raise Exception('Missing #define: %s:%d' % (header, index + 2))

      guard = entry[3].split(' ')[1]
      guard = guard.replace(root, '') if len(root) > 1 else guard
      content[index] = ('#ifndef %s\n' % guard).encode('utf-8')
      # Since cpplint does not print messages for the #define line, just
      # blindly overwrite the #define that was here.
      content[index + 1] = ('#define %s\n' % guard).encode('utf-8')
    elif msg.startswith(ENDIF_MSG_START):
      assert len(entry) == 3
      assert msg.endswith(ENDIF_MSG_END)

      with open(header, 'rb') as f:
        content = f.readlines()
      endif = msg[len(ENDIF_MSG_START):-len(ENDIF_MSG_END)]
      endif = endif.replace(root, '') if len(root) > 1 else endif
      content[index] = ('%s\n' % endif).encode('utf-8')
    elif msg == NO_GUARD_MSG:
      assert index == -1
      continue
    else:
      raise Exception('Unknown cpplint message: %s for %s:%s' %
                      (msg, header, line))

    with open(header, 'wb') as f:
      f.writelines(content)


if __name__ == '__main__':
  process_cpplint_recommendations(sys.stdin)
