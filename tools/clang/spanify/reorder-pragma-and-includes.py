#!/bin/env python3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to reorder the #pragma allow_unsafe_buffers and #include
# <array> in the files that have been modified by the current patch. This is
# done to allow clang format to better format the #includes as it can't reorder
# through the macros.

import os

modified_files = os.popen("git diff --name-only HEAD~..HEAD").read().split(
    "\n")
modified_files = list(filter(None, modified_files))

# Invert the #include <array> and the #pragma allow_unsafe_buffers. This allow
# clang format to better format the #includes as it can't reorder through the
# macros.
for file in modified_files:
    lines = []
    try:
        with open(file, 'r') as f:
            lines = f.readlines()
    except:
        continue  # Skip files that were deleted.

    # Do we have #include <array> above #pragma allow_unsafe_buffers?
    line_array = -1
    line_pragma = -1
    for i, line in enumerate(lines):
        if "#include <array>" in line:
            line_array = i
        if "#pragma allow_unsafe_buffers" in line:
            line_pragma = i

    if line_array == -1 or line_pragma == -1 or line_array > line_pragma:
        continue  # Nothing to reorder.

    # Reorder:
    with open(file, 'w') as f:
        in_opt_out = False
        for line in lines:
            # Write out all lines except for "#include <array>" until we see the
            # UNSAFE_BUFFERS_BUILD macro. Set |in_opt_out| at this point when
            # this occurs.
            if not in_opt_out:
                if not "#include <array>" in line:
                    f.write(line)
                if "#ifdef UNSAFE_BUFFERS_BUILD" in line:
                    in_opt_out = True
                continue

            # Write out each line until we hit the end of the
            # UNSAFE_BUFFERS_BUILD and afterwards write out the
            # "#include <array>".
            if in_opt_out:
                f.write(line)
                if "#endif" in line:
                    f.write("\n#include <array>\n")
                    in_opt_out = False
                continue

os.system("git cl format")
