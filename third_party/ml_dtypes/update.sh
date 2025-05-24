#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if [ $(basename ${PWD}) != "src" ]; then
  echo "Please set the current working directory to chromium/src first!"
  exit 1
fi

files=(
  "LICENSE"
  "ml_dtypes/include/float8.h"
  "ml_dtypes/include/intn.h"
  "ml_dtypes/include/mxfloat.h"
)

git clone --depth 1 https://github.com/jax-ml/ml_dtypes /tmp/ml_dtypes
rm -rf third_party/ml_dtypes/src/*
pushd third_party/ml_dtypes/src/ > /dev/null

for file in ${files[@]} ; do
  if [ ! -d "$(dirname ${file})" ] ; then
    mkdir -p "$(dirname ${file})"
  fi
  cp "/tmp/ml_dtypes/${file}" "${file}"
done

popd > /dev/null
rm -rf /tmp/ml_dtypes
