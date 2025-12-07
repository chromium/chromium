#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reads files, looks for #pragma allow_unsafe_buffers, and removes it.

The target block is of the form:
#ifdef UNSAFE_BUFFERS_BUILD
// Optional TODO comment...
#pragma allow_unsafe_buffers
#endif

If the block is surrounded by blank lines (both before and after),
one of the blank lines is also removed."""

import sys
import os


def process_file(filepath):
  try:
    with open(filepath, 'r') as f:
      lines = f.readlines()
  except Exception as e:
    print(f"Error: Could not read file {filepath}: {e}", file=sys.stderr)
    return

  # Keep a copy of the original lines to detect if changes were made
  original_line_count = len(lines)
  lines_to_write = []
  i = 0

  while i < len(lines):
    current_line = lines[i].strip()
    if current_line == "#ifdef UNSAFE_BUFFERS_BUILD":
      has_todo = (i + 1 < len(lines)
                  and lines[i + 1].strip().startswith("// TODO"))
      pragma_offset = 2 if has_todo else 1

      # Check if the pragma and endif lines exist and match the pattern
      pragma_line_index = i + pragma_offset
      pragma_found = (
          pragma_line_index < len(lines) and
          lines[pragma_line_index].strip().startswith("#pragma allow_unsafe_"))

      endif_line_index = i + pragma_offset + 1
      endif_found = (endif_line_index < len(lines)
                     and lines[endif_line_index].strip() == "#endif")

      if pragma_found and endif_found:
        block_end_line_index = endif_line_index
        line_before_is_blank = (i == 0) or (lines[i - 1].strip() == "")
        line_after_index = block_end_line_index + 1
        line_after_is_blank = (line_after_index >= len(lines)) or \
                              (lines[line_after_index].strip() == "")

        if line_before_is_blank and line_after_is_blank:
          i = line_after_index + 1
        else:
          i = block_end_line_index + 1
        continue

    lines_to_write.append(lines[i])
    i += 1

  if len(lines_to_write) != original_line_count:
    try:
      with open(filepath, 'w') as f:
        f.writelines(lines_to_write)
    except Exception as e:
      print(f"Error: Could not write to file {filepath}: {e}", file=sys.stderr)
  else:
    print(f"No matching block found in {filepath}. File unchanged.")


def main():
  if len(sys.argv) < 2:
    print(f"Usage: python {sys.argv[0]} <file1.cpp> <file2.cpp> ...")
    sys.exit(1)

  # Process each file provided on the command line
  for filename in sys.argv[1:]:
    if os.path.isfile(filename):
      process_file(filename)
    else:
      print(f"Error: File not found at '{filename}'", file=sys.stderr)


if __name__ == "__main__":
  main()
