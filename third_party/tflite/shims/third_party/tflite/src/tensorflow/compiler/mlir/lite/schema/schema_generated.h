// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_LITE_SCHEMA_SCHEMA_GENERATED_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_LITE_SCHEMA_SCHEMA_GENERATED_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/litert/src/tflite/schema/schema_generated.h"
#else
#include_next "tensorflow/compiler/mlir/lite/schema/schema_generated.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_LITE_SCHEMA_SCHEMA_GENERATED_H_
