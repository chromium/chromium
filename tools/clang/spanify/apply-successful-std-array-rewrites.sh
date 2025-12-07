#!/bin/bash

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# An example script to apply successful std::array rewrites.
#
# Prerequisites:
# --------------
# Populate ~/scratch/patch_{}.{out,diff,pass,fail} files by running:
# //tools/clang/scripts/evaluate_patches.py
# with patch_limit = 9999

echo "" > output.txt
for f in ~/scratch/patch_*.diff
do
    # Extract index from patch_{index}.diff
    index=$(echo $f | sed 's/.*patch_\(.*\).diff/\1/')

    if grep -q '^+.*std::array' $f || grep -q '^.*std::to_array' $f; then
        if [ -f ${f/.diff/.fail} ]; then
          continue
        fi

        echo "Processing patch_$index.diff"

        # Aggregate the replacements.
        cat ${f/.diff/.txt} >> output.txt

        # A new line to deal with the missing '\n' at the end of the file.
        echo "" >> output.txt
    fi
done

# The output contains lines with the format:
# r:::{0}:::{1}:::{2}:::{3}
# We need to sort numerically by {1}
# {0} is the file path
# {1} is the file offset
# {2} is the replacement length
# {3} is the replacement text

# We must sort by {1} numerically in descending order to avoid conflict when
# applying replacements.

# Use perl
perl -ane '
    $line = $_;
    @fields = split(/:::/, $line);
    print $fields[1] . " " . $line;
' output.txt \
  | sort -rn \
  | cut -d ' ' -f 2- \
  | ./tools/clang/scripts/apply_edits.py -p ./out/linux
