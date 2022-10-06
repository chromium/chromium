# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/sh

# Script to run Chrome Health A/A trustworthy experiments

export RELEASE_MILESTONE=$(curl -s https://chromiumdash.appspot.com/fetch_milestone_schedule?offset=-1 | jq '.mstones[].mstone')
echo "Release Milestone" $RELEASE_MILESTONE

export releaseBranchNo=$(curl -s https://chromiumdash.appspot.com/fetch_milestones | jq -r '.[] | select(.milestone | tostring == $ENV.RELEASE_MILESTONE) | .chromium_branch')
export query=$(echo "https://chromium.googlesource.com/chromium/src.git/+log/refs/branch-heads/${releaseBranchNo}?format=JSON")
export releaseHash=$(curl -s $query | sed '1d' | jq -r '[.log[] | (select(.message | index("Incrementing VERSION to"))) | .commit][0]')
echo "Release hash" $releaseHash

# Tip of branch A/A
for i in {1..100}
do
  echo $i
  ~/depot_tools/pinpoint experiment-telemetry-start --base-commit=$releaseHash --exp-commit=$releaseHash --presets-file ~/chromium/src/tools/perf/chrome-health-presets.yaml --preset=speedometer2_pgo --attempts=40;
done