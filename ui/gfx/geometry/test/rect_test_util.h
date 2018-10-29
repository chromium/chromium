// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TEST_RECT_TEST_UTIL_H_
#define UI_GFX_GEOMETRY_TEST_RECT_TEST_UTIL_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
namespace test {

testing::AssertionResult RectContains(const gfx::Rect& outer_rect,
                                      const gfx::Rect& inner_rect);

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TEST_RECT_TEST_UTIL_H_
