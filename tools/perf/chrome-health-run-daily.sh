# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/sh

# BEFORE YOU RUN THIS - In your ~ directory, execute the following. Note - this is named health_chromium so that cd chr<tab> works.
# mkdir health_chromium
# cd health_chromium
# fetch --nohooks chromium

releaseBranchNo=5112 #M104
pinnedReleaseMinusOne=a1711811edd74ff1cf2150f36ffa3b0dae40b17f #103.0.5060.53
pinnedMain=6ff741d380a7bfe8aafa4d0e6a8e84c46ddb4d39 #refs/heads/main@{#1018088}

cd ~/health_chromium/src
git fetch

# Current release branch
git checkout -b branch_$releaseBranchNo branch-heads/$releaseBranchNo
git checkout -f branch_$releaseBranchNo
git pull
headOfRelease=`git whatchanged --grep="Incrementing VERSION" --format="%H" -1 | head -n 1`
echo $headOfRelease

# main branch
git checkout -f main
git pull
headOfMain=`git whatchanged --grep="Updating trunk VERSION" --format="%H" -1 | head -n 1`

# M vs. M-1
~/depot_tools/pinpoint experiment-telemetry-start --base-commit=$pinnedReleaseMinusOne --exp-commit=$headOfRelease --presets-file ~/chromium/src/tools/perf/chrome-health-presets.yaml --preset=chrome_health --attempts=40
# Main
~/depot_tools/pinpoint experiment-telemetry-start --base-commit=$pinnedMain --exp-commit=$headOfMain --presets-file ~/chromium/src/tools/perf/chrome-health-presets.yaml --preset=chrome_health_pgo --attempts=40