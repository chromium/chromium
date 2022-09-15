# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/sh

# BEFORE YOU RUN THIS - In your ~ directory, execute the following. Note - this is named health_chromium so that cd chr<tab> works.
# mkdir health_chromium
# cd health_chromium
# fetch --nohooks chromium

# Script to run weekly A/A tests

releaseBranchNo=5112 #M104

cd ~/health_chromium/src
git fetch

# Current release branch
git checkout -b branch_$releaseBranchNo branch-heads/$releaseBranchNo
git checkout -f branch_$releaseBranchNo
git pull
headOfRelease=`git whatchanged --grep="Incrementing VERSION" --format="%H" -1 | head -n 1`
echo $headOfRelease

# M vs. M-1
for i in {1..200}
do
  ~/depot_tools/pinpoint experiment-telemetry-start --base-commit=$headOfRelease --exp-commit=$headOfRelease --presets-file ~/chromium/src/tools/perf/chrome-health-presets.yaml --preset=speedometer2_pgo --attempts=40;
done