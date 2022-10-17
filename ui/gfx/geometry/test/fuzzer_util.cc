// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/test/fuzzer_util.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <vector>

#include "base/check_op.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

Transform ConsumeTransform(FuzzedDataProvider& fuzz) {
  Transform transform;
  float matrix_data[16];
  if (fuzz.ConsumeBool() && fuzz.remaining_bytes() >= sizeof(matrix_data)) {
    size_t consumed = fuzz.ConsumeData(matrix_data, sizeof(matrix_data));
    CHECK_EQ(consumed, sizeof(matrix_data));
    transform = Transform::ColMajorF(matrix_data);
  }
  return transform;
}

}  // namespace gfx
