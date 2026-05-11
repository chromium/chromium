#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""An example script to apply successful std::array rewrites.

Prerequisites:
--------------
Populate <scratch>/patch_{}.{out,diff,pass,fail} files by running:
//tools/clang/spanify/evaluate_patches.py
with patch_limit = 9999
"""

import pathlib
import sys
from spanify_utils import apply_collected_edits, scratch_dir


def main():
    valid_patches = []

    for p in scratch_dir().glob('patch_*.pass'):
        index = int(p.name.removeprefix('patch_').removesuffix('.pass'))
        diff_file = scratch_dir() / f"patch_{index}.diff"
        if diff_file.exists():
            valid_patches.append((index, diff_file))

    valid_patches.sort(key=lambda x: x[0])

    replacements = []
    for index, p in valid_patches:
        # Check if patch added std::array or std::to_array
        has_std_array = False
        with p.open("r") as fd:
            for line in fd:
                if line.startswith('+') and ('std::array' in line
                                             or 'std::to_array' in line):
                    has_std_array = True
                    break

        if has_std_array:
            print(f"Processing patch_{index}.diff")
            txt_file = scratch_dir() / f"patch_{index}.txt"
            if txt_file.exists():
                replacements.extend(
                    txt_file.read_text().splitlines(keepends=True))
            else:
                print(f"Warning: {txt_file} does not exist.", file=sys.stderr)

    print(f"Found {len(valid_patches)} patches.")
    print(f"Collected {len(replacements)} replacements.")
    apply_collected_edits(replacements)
    return 0


if __name__ == "__main__":
    sys.exit(main())
