// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TEST_TRANSFORM_TEST_UTIL_H_
#define UI_GFX_GEOMETRY_TEST_TRANSFORM_TEST_UTIL_H_

#include "ui/gfx/transform.h"

namespace gfx {

// This is a function rather than a macro because when this is included as a
// macro in bulk, it causes a significant slow-down in compilation time. This
// problem exists with both gcc and clang, and bugs have been filed at
// http://llvm.org/bugs/show_bug.cgi?id=13651
// and http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54337
void ExpectTransformationMatrixEq(const Transform& expected,
                                  const Transform& actual);

void ExpectTransformationMatrixNear(const Transform& expected,
                                    const Transform& actual,
                                    float abs_error);

// Should be used in test code only, for convenience. Production code should use
// the gfx::Transform::GetInverse() API.
Transform InvertAndCheck(const Transform& transform);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TEST_TRANSFORM_TEST_UTIL_H_
