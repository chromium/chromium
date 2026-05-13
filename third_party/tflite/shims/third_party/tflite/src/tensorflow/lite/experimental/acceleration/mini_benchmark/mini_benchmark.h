// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_EXPERIMENTAL_ACCELERATION_MINI_BENCHMARK_MINI_BENCHMARK_H_
#define THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_EXPERIMENTAL_ACCELERATION_MINI_BENCHMARK_MINI_BENCHMARK_H_

#include "build/buildflag.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(USE_LITERT_TFLITE)
#include "third_party/litert/src/tflite/experimental/acceleration/mini_benchmark/mini_benchmark.h"
#else
#include_next "tensorflow/lite/experimental/acceleration/mini_benchmark/mini_benchmark.h"
#endif

#endif  // THIRD_PARTY_TFLITE_SHIMS_THIRD_PARTY_TFLITE_SRC_TENSORFLOW_LITE_EXPERIMENTAL_ACCELERATION_MINI_BENCHMARK_MINI_BENCHMARK_H_
