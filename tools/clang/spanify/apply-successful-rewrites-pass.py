#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""An example script to apply successful Spanifier rewrites.

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
        valid_patches.append(
            (int(p.name.removeprefix('patch_').removesuffix('.pass')), p))

    if not valid_patches:
        print(f"No successful patches found in {scratch_dir()}.",
              file=sys.stderr)
        return 0

    valid_patches.sort(key=lambda x: x[0])

    replacements = []
    for index, p in valid_patches:
        txt_file = scratch_dir() / f"patch_{index}.txt"
        if txt_file.exists():
            print(f"Processing patch_{index}")
            replacements.extend(txt_file.read_text().splitlines(keepends=True))

    if replacements:
        apply_collected_edits(replacements)
        return 0
    else:
        print("No replacements to apply.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
