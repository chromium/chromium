#!/bin/bash -e
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Downloads all logs from a CQ run. You can find the task ID by e.g. clicking
# through to the CQ run from gerrit and finding, e.g. the line
#
# Swarming Task: 4cb6401085894f10
#
# This will create a .txt for the main log and create subdir for the ID and put
# one .txt file in there for each subtask's log.
#
# Usage:
#   get_all.sh <output_dir> <task ID>

base_dir=$1
shift
id=$1
shift

bindir=$(dirname $0)

function get_ids() {
  perl -lne 'print $1 if m#shard \#0\]\(https://chromium-swarm.appspot.com/user/task/([0-9a-f]*)#;' "$1" | sort | uniq
}

log=$("$bindir"/get_one.sh "$base_dir" "$id")
dir="$base_dir/$id"
running=0
for id in $(get_ids "$log" ); do
  if [ "$running" -gt 32 ]; then
    echo >&2 waiting $running
    wait -n
    running=$(($running - 1))
  fi
  echo >&2 getting $id
  "$bindir"/get_one.sh "$dir" "$id" &
  running=$(($running + 1))
done

while [ "$running" -gt 0 ]; do
  echo >&2 waiting $running
  wait -n
  running=$(($running - 1))
done
