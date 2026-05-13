// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_LITE_CORE_MODEL_BUILDER_BASE_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_LITE_CORE_MODEL_BUILDER_BASE_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/litert/src/tflite/core/model_builder_base.h"
#else
#include_next "tensorflow/compiler/mlir/lite/core/model_builder_base.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_LITE_CORE_MODEL_BUILDER_BASE_H_
