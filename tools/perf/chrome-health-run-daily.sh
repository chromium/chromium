# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/sh

# Script to run Chrome Health Nightly A/B tests

# get stable hash
versionHistoryURL='https://versionhistory.googleapis.com/v1/chrome/platforms/win64/channels/stable/versions/all/releases?filter=endtime=none'

export fractionGroup=$(curl -s $versionHistoryURL | jq -r --sort-keys '[(.releases[].fractionGroup)][0]')
export highestFraction=$(curl -s $versionHistoryURL | jq -r --sort-keys '[.releases[] | select(.fractionGroup == $ENV.fractionGroup) | .fraction][0]')
export stableVersion=$(curl -s $versionHistoryURL | jq -r --sort-keys '[.releases[] | select(.fractionGroup == $ENV.fractionGroup) | select(.fraction == ($ENV.highestFraction|tonumber)) | .version][0]')
echo "Stable version" $stableVersion

export stableBranch=$(echo $stableVersion | cut -d'.' -f 3)
echo $stableBranch

export query=$(echo "https://chromium.googlesource.com/chromium/src.git/+log/refs/branch-heads/${stableBranch}?format=JSON")
export versionQuery=$(echo "Incrementing VERSION to ${stableVersion}")
export stableHash=$(curl -s $query | sed '1d' | jq -r '.log[] | (select(.message | index($ENV.versionQuery))) | .commit')
echo "Stable hash" $stableHash


# get release hash
export RELEASE_MILESTONE=$(curl -s https://chromiumdash.appspot.com/fetch_milestone_schedule?offset=-1 | jq '.mstones[].mstone')
echo "Release Milestone" $RELEASE_MILESTONE

export releaseBranchNo=$(curl -s https://chromiumdash.appspot.com/fetch_milestones | jq -r '.[] | select(.milestone | tostring == $ENV.RELEASE_MILESTONE) | .chromium_branch')

export query=$(echo "https://chromium.googlesource.com/chromium/src.git/+log/refs/branch-heads/${releaseBranchNo}?format=JSON")
export releaseHash=$(curl -s $query | sed '1d' | jq -r '[.log[] | (select(.message | index("Incrementing VERSION to"))) | .commit][0]')
echo "Release hash" $releaseHash


# Run Chrome Health Nightly experiment
# M vs. M-1
~/Projects/depot_tools/pinpoint experiment-telemetry-start --base-commit=$stableHash --exp-commit=$releaseHash --presets-file ~/Projects/chromium/src/tools/perf/chrome-health-presets.yaml --preset=speedometer2 --attempts=40
