#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A temporary script to evaluate how many patches produced by the tool compile.

# Setup a build directory to evaluate the patches. This is common to all the
# patches to avoid recompiling the entire project for each patch.
gn gen out/linux
echo "use_remoteexec = true " > out/linux/args.gn

# Produce a full rewrite, and store individual patches below ~/scratch/patch_*
./tools/clang/spanify/rewrite-multiple-platforms.sh --platforms=linux
git reset --hard HEAD # Reset the changes made by the tool.

patch_count=$(ls -1 ~/scratch/patch_* | wc -l)

# This file will store a summary of each patch evaluation.
# ```
# Patch 0 is passing
# Patch 1 is failing
# Patch 2 is passing
# ...
# ```
echo "" > ~/scratch/patch_evaluation.txt

# Create the patches.
patch_index=0
for patch in ~/scratch/patch_*
do
  echo "Producing patch $patch_index/$patch_count"

  # Apply the edits in a new branch.
  # They won't be removed after the evaluation, so that we can inspect them.
  # They share a common prefix to allow removing them all at once.
  git branch -D spanification_rewrite_evaluate_$patch_index
  git new-branch spanification_rewrite_evaluate_$patch_index
  cat $patch | tools/clang/scripts/apply_edits.py -p ./out/linux/

  # Commit the changes.
  git add -u
  echo "spanification patch $patch_index applied" > commit_message.txt
  echo "" >> commit_message.txt
  echo "This is an automated patch produced by the spanification tool." \
    >> commit_message.txt
  echo "Patch: $patch_index" >> commit_message.txt
  git commit -F commit_message.txt

  # Build the change, and report back if it fails.
  echo "Evaluating patch $patch_index/patch_count"
  gn gen out/linux
  autoninja -C out/linux 2>&1 | tee ~/scratch/output_$patch_index.txt
  error=$(grep -i "subcommand failed" ~/scratch/output_$patch_index.txt)
  if [ -z "$error" ];
  then
    echo "Patch $patch_index is passing" >> ~/scratch/patch_evaluation.txt
  else
    echo "Patch $patch_index is failing" >> ~/scratch/patch_evaluation.txt
  fi

  patch_index=$((patch_index+1))
done
