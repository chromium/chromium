// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_BENCHMARK_H_
#define JNI_ZERO_BENCHMARK_H_

#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero::benchmark {

class NativeBenchmarkObject {
 public:
  NativeBenchmarkObject() = default;
  ~NativeBenchmarkObject() = default;
};

}  // namespace jni_zero::benchmark

#endif  // JNI_ZERO_BENCHMARK_H_
