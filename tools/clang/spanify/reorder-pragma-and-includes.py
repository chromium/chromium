#!/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to reorder the #pragma allow_unsafe_buffers and #include
# <array> in the files that have been modified by the current patch. This is
# done to allow clang format to better format the #includes as it can't reorder
# through the macros.

import sys
import os

INCLUDE_ARRAY = "#include <array>"
INCLUDE_SPAN = '#include "base/containers/span.h"'
INCLUDE_RAW_SPAN = '#include "base/memory/raw_span.h"'


class ReorderTarget:

    def __find_line_numbers(self):
        # Do we have any `#include`s above `#pragma allow_unsafe_buffers`?
        for i, unstripped_line in enumerate(self.lines):
            line = unstripped_line.strip()
            if line == INCLUDE_ARRAY:
                self.array_include_line = i
            elif line == INCLUDE_SPAN:
                self.span_include_line = i
            elif line == INCLUDE_RAW_SPAN:
                self.raw_span_include_line = i
            elif "#pragma allow_unsafe_buffers" in line:
                self.pragma_line = i

    def __init__(self, path):
        self.lines = None
        self.array_include_line = None
        self.span_include_line = None
        self.raw_span_include_line = None
        self.pragma_line = None
        try:
            with open(path, 'r') as f:
                self.lines = f.readlines()
        except FileNotFoundError:
            return  # Skip files that were deleted.
        self.__find_line_numbers()

    def _should_reorder_impl(self, member):
        try:
            return member < self.pragma_line
        except TypeError:
            # One or both are `None`.
            return False

    def should_reorder_array_include(self):
        return self._should_reorder_impl(self.array_include_line)

    def should_reorder_span_include(self):
        return self._should_reorder_impl(self.span_include_line)

    def should_reorder_raw_span_include(self):
        return self._should_reorder_impl(self.raw_span_include_line)

    def should_reorder(self):
        # Deleted file.
        if self.lines is None:
            return False
        # No pragma? Then `git cl format` shouldn't be confused.
        if self.pragma_line is None:
            return False
        return (self.should_reorder_array_include()
                or self.should_reorder_span_include()
                or self.should_reorder_raw_span_include())


def reorder_pragma_and_includes(path):
    target = ReorderTarget(path)
    if not target.should_reorder():
        return

    # Entering this block means there _is_ something to reorder.
    # 1.  The `#pragma` line exists. We _will_ pass through it as
    #     we traverse the file.
    # 2.  Either `span.h` or `<array>` is included - possibly both.
    with open(path, 'w') as f:
        in_opt_out = False
        for (line_number, line) in enumerate(target.lines):
            # Write out all lines except for the overly-high-up `#include`s
            # until we step inside the `UNSAFE_BUFFERS_BUILD` macro. Set
            # `in_opt_out` at the point when this occurs.
            if not in_opt_out and line_number < target.pragma_line:
                if line.strip() not in (INCLUDE_ARRAY, INCLUDE_SPAN,
                                        INCLUDE_RAW_SPAN):
                    f.write(line)
                if "#ifdef UNSAFE_BUFFERS_BUILD" in line:
                    in_opt_out = True
                continue

            # Write out each line until we hit the end of the
            # `UNSAFE_BUFFERS_BUILD` and afterwards write out the
            # necessary `#include`s.
            if in_opt_out:
                f.write(line)
                if "#endif" in line:
                    if target.should_reorder_array_include():
                        f.write(f"\n{INCLUDE_ARRAY}\n")
                    if target.should_reorder_span_include():
                        f.write(f"\n{INCLUDE_SPAN}\n")
                    if target.should_reorder_raw_span_include():
                        f.write(f"\n{INCLUDE_RAW_SPAN}\n")
                    in_opt_out = False
                continue

            # We have passed the `#pragma` and can mindlessly spit out
            # every subsequent line.
            assert not in_opt_out and line_number > target.pragma_line
            f.write(line)


def main():
    modified_files = [
        f for f in os.popen("git diff --name-only HEAD~..HEAD").read().split(
            "\n") if f
    ]

    for file in modified_files:
        reorder_pragma_and_includes(file)

    os.system("git cl format")


if __name__ == "__main__":
    sys.exit(main())
