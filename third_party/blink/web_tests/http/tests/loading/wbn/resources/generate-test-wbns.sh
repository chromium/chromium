#!/bin/sh

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go install github.com/WICG/webpackage/go/bundle/cmd/...@latest"
    echo '  export PATH=$PATH:$(go env GOPATH)/bin'
    exit 1
fi

gen-bundle \
  -version b2 \
  -baseURL http://127.0.0.1:8000/loading/wbn/resources/wbn/server/hello/ \
  -primaryURL http://127.0.0.1:8000/loading/wbn/resources/wbn/server/hello/script.js \
  -dir hello/ \
  -o wbn/hello.wbn

cp wbn/hello.wbn wbn/hello.wbn-without-nosniff
cp wbn/hello.wbn wbn/hello.wbn-wrong-mime-type

gen-bundle \
  -version b2 \
  -baseURL http://127.0.0.1:8000/loading/wbn/resources/wbn/ \
  -primaryURL http://127.0.0.1:8000/loading/wbn/resources/wbn/empty.js \
  -dir empty-resource/ \
  -o wbn/empty-resource.wbn
