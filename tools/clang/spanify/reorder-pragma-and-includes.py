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


class ReorderTarget:

    def __find_line_numbers(self):
        # Do we have any `#include`s above
        # * `#pragma allow_unsafe_buffers` or
        # * the header guard (if in a header file)?
        in_opt_out = False
        guard_line = None
        pragma_end = None
        for i, unstripped_line in enumerate(self.lines):
            line = unstripped_line.strip()
            if line in self.lines_to_reorder:
                # If we come across a duplicate `#include`, it's
                # probably an existing one, and we should leave it alone.
                # The `#include`s that spanify emits should be the
                # highest-up.
                if self.lines_to_reorder[line] is not None:
                    continue
                self.lines_to_reorder[line] = i
            elif '#ifdef UNSAFE_BUFFERS_BUILD' in line:
                in_opt_out = True
            elif in_opt_out and '#endif' in line:
                pragma_end = i
                in_opt_out = False
            elif line == self.guard_format:
                guard_line = i

        # If we have both a pragma and a guard, we want to insert _after_ both.
        # However if we only have either pragma or guard we insert after
        # whichever is present.
        try:
            self.insertion_point = max(pragma_end, guard_line)
        except TypeError:
            self.insertion_point = pragma_end or guard_line
        if self.insertion_point is None:
            return
        self.lines_to_reorder = {
            k: v
            for (k, v) in self.lines_to_reorder.items()
            if v is not None and v < self.insertion_point
        }

    def __init__(self, path):
        self.lines = None
        self.lines_to_reorder = {
            '#include <array>': None,
            '#include <cstdint>': None,
            '#include "base/containers/auto_spanification_helper.h"': None,
            '#include "base/containers/span.h"': None,
            '#include "base/memory/raw_span.h"': None,
            '#include "base/numerics/safe_conversions.h"': None,
        }
        self.insertion_point = None
        self.guard_format = self._compute_guard_format(path)

        try:
            with open(path, 'r') as f:
                self.lines = f.readlines()
        except FileNotFoundError:
            return  # Skip files that were deleted.
        self.__find_line_numbers()

    def _compute_guard_format(self, path):
        # The guard format is the path to the file with underscores instead of
        # slashes and in uppercase with a trailing underscore.
        guard_format = path.upper().replace('/', '_').replace('.', '_') + '_'
        return f'#define {guard_format}'

    def should_reorder(self):
        # Deleted file.
        if self.lines is None:
            return False
        # If there were no pragmas or header guards, then
        # `git cl format` should not be confused.
        if self.insertion_point is None:
            return False
        return bool(self.lines_to_reorder)


def reorder_pragma_and_includes(path):
    target = ReorderTarget(path)
    if not target.should_reorder():
        return

    # Entering this block means there _is_ something to reorder.
    # 1.  The `#pragma` line exists. We _will_ pass through it as
    #     we traverse the file.
    # 2.  `target.lines_to_reorder` is a nonempty dict.
    with open(path, 'w') as f:
        for (line_number, line) in enumerate(target.lines):
            # Write out all lines except for the overly-high-up `#include`s
            # until we pass the the `UNSAFE_BUFFERS_BUILD` macro and the HEADER
            # guards (if present).
            if line_number < target.insertion_point:
                if line.strip() not in target.lines_to_reorder:
                    f.write(line)
                continue

            if line_number == target.insertion_point:
                f.write(line)
                for to_reorder in target.lines_to_reorder:
                    f.write("\n")
                    f.write(to_reorder)
                    f.write("\n")
                continue

            # We have passed the `#pragma` and any header guards (if present)
            # and can mindlessly spit out every subsequent line.
            assert line_number > target.insertion_point
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
