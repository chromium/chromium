# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def GetHighlight(line, error):
  """Returns the substring of |line| that is highlighted in |error|."""
  error_lines = error.split('\n')
  # TODO(dschuyler): Splitting the error on \n will prevent index(line)
  # from finding the line. As a workaround, return the whole, unfiltered
  # line.
  if line not in error_lines:
    return line
  highlight = error_lines[error_lines.index(line) + 1]
  return ''.join(ch1 for (ch1, ch2) in zip(line, highlight) if ch2 == '^')
