// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_TENSORFLOW_LITE_DELEGATES_XNNPACK_WEIGHT_CACHE_SCHEMA_GENERATED_H_
#define THIRD_PARTY_TFLITE_SHIMS_TENSORFLOW_LITE_DELEGATES_XNNPACK_WEIGHT_CACHE_SCHEMA_GENERATED_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "tflite/delegates/xnnpack/weight_cache_schema_generated.h"
#else
#include_next "tensorflow/lite/delegates/xnnpack/weight_cache_schema_generated.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_TENSORFLOW_LITE_DELEGATES_XNNPACK_WEIGHT_CACHE_SCHEMA_GENERATED_H_
