#!/bin/bash
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

URL_BASE='https://gerrit.googlesource.com/gitiles/+/HEAD/gitiles-servlet/src/main/resources/com/google/gitiles/static'

# Quickly pull down the latest gitiles css files.
for css in base doc prettify/prettify; do
  output="${css#*/}.css"
  url="${URL_BASE}/${css}.css?format=TEXT"
  echo "Updating ${output}"
  curl "${url}" | base64 -d >"${output}"
done
