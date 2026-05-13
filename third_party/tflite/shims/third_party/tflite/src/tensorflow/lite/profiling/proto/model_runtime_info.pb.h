// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_PROFILING_PROTO_MODEL_RUNTIME_INFO_PB_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_PROFILING_PROTO_MODEL_RUNTIME_INFO_PB_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/litert/src/tflite/profiling/proto/model_runtime_info.pb.h"
#else
#include_next "tensorflow/lite/profiling/proto/model_runtime_info.pb.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_PROFILING_PROTO_MODEL_RUNTIME_INFO_PB_H_
