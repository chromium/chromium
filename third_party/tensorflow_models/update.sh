#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ $(basename ${PWD}) != "src" ]; then
  echo "Please set the current working directory to chromium/src first!"
  exit 1
fi

files=(
  "LICENSE"
  "research/seq_flow_lite/tf_ops/projection_normalizer_util.cc"
  "research/seq_flow_lite/tf_ops/projection_normalizer_util.h"
  "research/seq_flow_lite/tf_ops/projection_util.cc"
  "research/seq_flow_lite/tf_ops/projection_util.h"
  "research/seq_flow_lite/tf_ops/skipgram_finder.h"
  "research/seq_flow_lite/tf_ops/skipgram_finder.cc"
  "research/seq_flow_lite/tflite_ops/denylist.cc"
  "research/seq_flow_lite/tflite_ops/denylist.h"
  "research/seq_flow_lite/tflite_ops/denylist_skipgram.cc"
  "research/seq_flow_lite/tflite_ops/denylist_skipgram.h"
  "research/seq_flow_lite/tflite_ops/quantization_util.h"
  "research/seq_flow_lite/tflite_ops/sequence_string_projection.cc"
  "research/seq_flow_lite/tflite_ops/sequence_string_projection.h"
  "research/seq_flow_lite/tflite_ops/tflite_qrnn_pooling.cc"
  "research/seq_flow_lite/tflite_ops/tflite_qrnn_pooling.h"
)

git clone --depth 1 https://github.com/tensorflow/models /tmp/models
rm -rf third_party/tensorflow_models/src/*
pushd third_party/tensorflow_models/src/

for file in ${files[@]} ; do
  if [ ! -d "$(dirname ${file})" ] ; then
    mkdir -p "$(dirname ${file})"
  fi
  cp "/tmp/models/${file}" "${file}"
done

popd
rm -rf /tmp/models
