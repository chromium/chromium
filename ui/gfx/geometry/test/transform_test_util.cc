// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/test/transform_test_util.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

// NOTE: even though transform data types use double precision, we only check
// for equality within single-precision error bounds because many transforms
// originate from single-precision data types such as quads/rects/etc.

void ExpectTransformationMatrixEq(const Transform& expected,
                                  const Transform& actual) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_FLOAT_EQ(expected.matrix().get(row, col),
                      actual.matrix().get(row, col))
          << "row: " << row << " col: " << col;
    }
  }
}

void ExpectTransformationMatrixNear(const Transform& expected,
                                    const Transform& actual,
                                    float abs_error) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_NEAR(expected.matrix().get(row, col),
                  actual.matrix().get(row, col), abs_error)
          << "row: " << row << " col: " << col;
    }
  }
}

Transform InvertAndCheck(const Transform& transform) {
  Transform result(Transform::kSkipInitialization);
  bool inverted_successfully = transform.GetInverse(&result);
  DCHECK(inverted_successfully);
  return result;
}

}  // namespace gfx
