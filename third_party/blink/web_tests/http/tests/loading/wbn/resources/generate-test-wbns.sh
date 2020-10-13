#!/bin/sh

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
    exit 1
fi

gen-bundle \
  -version b1 \
  -baseURL http://127.0.0.1:8000/loading/wbn/resources/wbn/server/hello/ \
  -primaryURL http://127.0.0.1:8000/loading/wbn/resources/wbn/server/hello/script.js \
  -dir hello/ \
  -o wbn/hello.wbn

gen-bundle \
  -version b1 \
  -baseURL https://localhost:8443/loading/wbn/resources/wbn/server/wbn-subresource-origin-trial/ \
  -primaryURL https://localhost:8443/loading/wbn/resources/wbn/server/wbn-subresource-origin-trial/script.js \
  -dir wbn-subresource-origin-trial/ \
  -o wbn/wbn-subresource-origin-trial.wbn

gen-bundle \
  -version b1 \
  -baseURL https://localhost:8443/loading/wbn/resources/wbn/server/wbn-subresource-third-party-origin-trial/ \
  -primaryURL https://localhost:8443/loading/wbn/resources/wbn/server/wbn-subresource-third-party-origin-trial/script.js \
  -dir wbn-subresource-third-party-origin-trial/ \
  -o wbn/wbn-subresource-third-party-origin-trial.wbn

gen-bundle \
  -version b1 \
  -baseURL http://127.0.0.1:8000/loading/wbn/resources/wbn/ \
  -primaryURL http://127.0.0.1:8000/loading/wbn/resources/wbn/empty.js \
  -dir empty-resource/ \
  -o wbn/empty-resource.wbn
