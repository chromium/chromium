#!/usr/bin/env python
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import re
import os

import range_merger

def apply_fixes(file_path, fixes):
  """Applies the collected fixes to the given file.
  Fixes are (start_line, start_col, end_line, end_col) 0-indexed tuples.
  This function modifies the file in place."""
  try:
    # Read file content
    with open(file_path, 'r') as f:
      lines = f.readlines()
  except IOError as e:
    sys.stderr.write(f"Error: Could not read file '{file_path}': {e}\n")
    return

  # Remove trailing newlines from lines to simplify string manipulation.
  # Newlines will be added back when writing the modified content.
  lines = [line.rstrip('\n') for line in lines]

  # Combine adjacent and overlapping fixes
  fixes = range_merger.merge_ranges(fixes)

  # Sort fixes in reverse order (from end of file to beginning).
  # This is crucial to prevent earlier insertions from invalidating
  # the line and column indices of later insertions.
  fixes.sort(key=lambda x: (x[0], x[1]), reverse=True)

  for fix in fixes:
    start_line, start_col, end_line, end_col = fix

    # Define the macros to be inserted
    prefix = "UNSAFE_TODO("
    suffix = ")"

    # Ensure the start_line is within the bounds of the file content
    if not (0 <= start_line < len(lines)):
      sys.stderr.write(
          f"Warning: Fix for '{file_path}' has out-of-bounds start line "
          f"({start_line + 1}). Skipping fix {fix}.\n")
      continue

    # Ensure start_col is within the bounds of the specific line
    # Use <= because insertion can be at the end of the line
    if not (0 <= start_col <= len(lines[start_line])):
      sys.stderr.write(
          f"Warning: Fix for '{file_path}' on line {start_line + 1} has "
          f"out-of-bounds start column ({start_col + 1}). Skipping fix "
          f"{fix}.\n")
      continue

    # Ensure end_line is within bounds
    if not (0 <= end_line < len(lines)):
      sys.stderr.write(
          f"Warning: Fix for '{file_path}' has out-of-bounds end line "
          f"({end_line + 1}). Skipping fix {fix}.\n")
      continue

    # Ensure end_col_adjusted is within the bounds of the specific line for.
    # Use <= because insertion can be at the end of the line.
    if not (0 <= end_col <= len(lines[end_line])):
      sys.stderr.write(
          f"Warning: Fix for '{file_path}' on line {end_line + 1} has "
          f"out-of-bounds end column ({end_col + 1}). Skipping "
          f"suffix for fix {fix}.\n")
      continue

    # Insert the suffix after the character at end_col_adjusted
    lines[end_line] = (lines[end_line][:end_col] + suffix +
                       lines[end_line][end_col:])

    # Insert the prefix before the character at start_col
    lines[start_line] = (lines[start_line][:start_col] + prefix +
                         lines[start_line][start_col:])

  try:
    # Write the modified content back to the file
    with open(file_path, 'w') as f:
      for line in lines:
        f.write(line + '\n')  # Re-add newline
    sys.stderr.write(f"Successfully applied fixes to '{file_path}'\n")
  except IOError as e:
    sys.stderr.write(f"Error: Could not write to file '{file_path}': {e}\n")


def main():
  all_corrections = {}

  # Matches the error start line: file:line:col:{range}: error: ...unsafe...
  error_start_re = re.compile(
      rb"^../../(.*?):(\d+):(\d+):\{(\d+):(\d+)-(\d+):(\d+)\}: "
      rb"(warning|error): .*unsafe.*")

  # Read compiler output line by line from standard input
  for line in sys.stdin.buffer.read().splitlines():
    match = error_start_re.match(line)
    if match:
      # Canonicalize the file path to handle relative paths consistently
      abs_file_path = os.path.abspath(match.group(1).decode('ascii'))

      # Convert to zero-based indices
      source_range = (int(match.group(4)) - 1, int(match.group(5)) - 1,
                      int(match.group(6)) - 1, int(match.group(7)) - 1)

      entry = all_corrections.setdefault(abs_file_path, [])
      entry.append(source_range)

  # After parsing all input from stdin, apply the collected fixes to the files
  for file_path, fixes_list in all_corrections.items():
    apply_fixes(file_path, fixes_list)


if __name__ == "__main__":
  main()
