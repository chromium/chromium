// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Handle wl_output object.
class TestOutput : public GlobalObject {
 public:
  TestOutput();
  ~TestOutput() override;
  void SetRect(const gfx::Rect rect) { rect_ = rect; }
  const gfx::Rect GetRect() { return rect_; }
  void OnBind() override;

  void SetScale(int32_t factor);

 private:
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(TestOutput);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
