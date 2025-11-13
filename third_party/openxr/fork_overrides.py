#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fork OpenXR Overrides

This script copies and overwrites all files in `src_overrides/src` from
`src/src` (excluding .clang-format), to prepare for "forking" the files again
and applying the patches from `src_overrides/patches`. It is recommended to
run this script, commit the forked files, and then attempt to apply the patches
in case there are any conflicts to fix via `git am src_overrides/patches/*`.
"""

import os
import shutil
from pathlib import Path

# Define the directory where this script is located, this way the script can be
# run from anywhere.
parent_root = Path(__file__).resolve().parent
source_root = parent_root / 'src/src'
override_root = parent_root / 'src_overrides/src'

if not source_root.exists():
    print(f'ERROR: Source directory missing: {source_root}')
    exit(1)

if not override_root.exists():
    print(f'WARNING: Override directory missing: {override_root}')
    exit(1)

# Use os.walk for robust directory traversal
for root, _, files in os.walk(override_root):
    current_root = Path(root)

    # Calculate the relative path using pathlib
    relative_dir = current_root.relative_to(override_root)

    for filename in files:
        if filename == '.clang-format':
            continue

        dest_path = current_root / filename
        source_path = source_root / relative_dir / filename

        if source_path.is_file():
            try:
                shutil.copy2(source_path, dest_path)
            except Exception as e:
                print(f'ERROR syncing {relative_dir / filename}: {e}')
        else:
            print(f'Source file not found (Skipped): {relative_dir / filename}')

print('Synchronization complete.')
