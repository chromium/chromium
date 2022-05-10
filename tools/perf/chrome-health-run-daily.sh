# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#!/bin/sh

releaseBranchNo=5005
pinnedReleaseMinusOne=93c720db8323b3ec10d056025ab95c23a31997c9 #101.0.4951.41
pinnedMain=6ee574c7eb5719153bbe0d1eff07fd0acbd864cc #refs/heads/main@{#966041}

cd ~/chromium/src

gclient sync --with_branch_heads --with_tags
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
~/depot_tools/pinpoint experiment-telemetry-start --base-commit=$pinnedMain --exp-commit=$headOfMain --presets-file ~/chromium/src/tools/perf/chrome-health-presets.yaml --preset=chrome_health --attempts=40