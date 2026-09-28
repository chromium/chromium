// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "tensorflow/lite/core/create_op_resolver.h"
#include "tensorflow/lite/mutable_op_resolver.h"

namespace tflite {

// Provides a no-op implementation of tflite::CreateOpResolver() for the
// `tflite_framework` target (which excludes builtin CPU op kernels and XNNPACK).
//
// `TfLiteInterpreterCreate()` in `tensorflow/lite/core/c/c_api.cc` calls
// `tflite::CreateOpResolver()`. The default upstream implementation
// (`create_op_resolver_with_builtin_ops.cc`) returns `BuiltinOpResolver`, which
// pulls in `register.cc` and all 135+ builtin CPU op kernel implementations.
// Returning an empty `MutableOpResolver` satisfies the linker symbol for
// `c_api.cc` without pulling in any builtin CPU op kernels.
std::unique_ptr<MutableOpResolver> CreateOpResolver() {
  return std::make_unique<MutableOpResolver>();
}

}  // namespace tflite
