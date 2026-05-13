// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_DELEGATES_GPU_COMMON_GOOGLE_KEEP_IF_MAX_2D_PT2_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_DELEGATES_GPU_COMMON_GOOGLE_KEEP_IF_MAX_2D_PT2_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/litert/src/tflite/delegates/gpu/common/google/keep_if_max_2d_pt2.h"
#else
#include_next "tensorflow/lite/delegates/gpu/common/google/keep_if_max_2d_pt2.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_DELEGATES_GPU_COMMON_GOOGLE_KEEP_IF_MAX_2D_PT2_H_
