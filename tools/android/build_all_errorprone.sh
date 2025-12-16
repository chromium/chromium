#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [[ ! -f build.ninja ]]; then
  echo "Please run from your output directory."
  exit 1
fi

if grep -q '^android_static_analysis.*off' args.gn; then
  echo 'Detected android_static_analysis="off" in args.gn'
  exit 1
fi

if ! grep -q '^android_static_analysis.*on' args.gn; then
  echo 'Probably want to add android_static_analysis="on" to enable remote execution.'
fi

exec autoninja $(siso query targets | grep __errorprone | sed -ne 's/: phony//p')
