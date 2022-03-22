# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/sh

releaseBranchNo=4844
releaseBranchMinusOneNo=4758
pinnedReleaseMinusOne=199fc4f2ce08413e0126f4e98393232412a76ab6 #98.0.4758.82
pinnedMain=6ee574c7eb5719153bbe0d1eff07fd0acbd864cc #refs/heads/main@{#966041}

cd ~/chromium/src

# Uncomment these two lines for the first run after updating one of the releaseBranchNos
#gclient sync --with_branch_heads --with_tags
#git fetch

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
pinpoint experiment-telemetry-start --base-commit=$pinnedReleaseMinusOne --exp-commit=$headOfRelease --presets-file tools/perf/chrome-health-presets.yaml --preset=chrome_health
# Main
pinpoint experiment-telemetry-start --base-commit=$pinnedMain --exp-commit=$headOfMain --presets-file tools/perf/chrome-health-presets.yaml --preset=chrome_health
# A/A
pinpoint experiment-telemetry-start --base-commit=$headOfMain --exp-commit=$headOfMain --presets-file tools/perf/chrome-health-presets.yaml --preset=chrome_health