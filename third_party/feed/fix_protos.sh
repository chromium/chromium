#!/bin/bash
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fixes protobufs references in proto_sources.gni.
PROTO_SOURCES="third_party/feed/proto_sources.gni"
if ! [[ -f "$PROTO_SOURCES" ]]; then
  echo "Cannot locate: $PROTO_SOURCES"
  exit 1
fi

# Exit when any command fails.
set -e

TEMP_FILE="third_party/feed/temp_proto.txt"
if [[ -f "$TEMP_FILE" ]]; then
	rm $TEMP_FILE
fi
sed -i '/proto".$/d' $PROTO_SOURCES
find third_party/feed/src | grep \\.proto$ | env LC_COLLATE=en_US.ASCII sort | sed 's/^third_party.feed.\(.*\)/  "\1",/g' > $TEMP_FILE
# It seems OK to have double quotes here so that $TEMP_FILE gets expanded.
sed -i "/\[/r $TEMP_FILE" $PROTO_SOURCES
rm $TEMP_FILE
echo "proto_sources.gni updated successfully"
