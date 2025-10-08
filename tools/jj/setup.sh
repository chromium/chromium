#!/bin/bash

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Repo root
cd "$(dirname ${BASH_SOURCE[0]})/../.."

if [[ ! -d .jj ]]; then
  jj git init --colocate .
  ln -sf "$(realpath tools/jj/config.toml)" .jj/repo/config.toml
fi

# Ensure that jj snapshots your current commit so it doesn't get lost with git
# switch.
jj new

# Fix issues with line endings. See go/jj-in-chromium.
git config core.autocrlf false
git switch origin/main --detach
jj abandon
git add -A

echo "Reminder: If you haven't already, we recommend joining https://groups.google.com/g/chromium-jj-users"

