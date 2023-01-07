#!/bin/bash -ex
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Download the log from a CQ run. You can find the task ID by e.g. clicking
# through to the CQ run from gerrit and finding, e.g. the line
#
# Swarming Task: 4cb6401085894f10
#
# The script will create a .txt for this log in the output dir. If you want all
# the logs of subtasks, see get_all.sh
#
# Usage:
#   get_one.sh <output_dir> <task ID>

base_dir=$1
shift
id=$1
shift

out="$base_dir/$id.txt"
mkdir -p "$base_dir"
tools/luci-go/swarming collect -task-output-stdout=console \
    -S chromium-swarm.appspot.com "$id" > "$out"
echo "$out"
