// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_QUANTIZATION_STABLEHLO_QUANTIZATION_CONFIG_PB_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_QUANTIZATION_STABLEHLO_QUANTIZATION_CONFIG_PB_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/tflite/src/tensorflow/compiler/mlir/quantization/stablehlo/quantization_config.pb.h"
#else
#include_next "tensorflow/compiler/mlir/quantization/stablehlo/quantization_config.pb.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_COMPILER_MLIR_QUANTIZATION_STABLEHLO_QUANTIZATION_CONFIG_PB_H_
