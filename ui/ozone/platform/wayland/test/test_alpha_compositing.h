// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ALPHA_COMPOSITING_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ALPHA_COMPOSITING_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manage wl_viewporter object.
class TestAlphaCompositing : public GlobalObject {
 public:
  TestAlphaCompositing();
  TestAlphaCompositing(const TestAlphaCompositing& rhs) = delete;
  TestAlphaCompositing& operator=(const TestAlphaCompositing& rhs) = delete;
  ~TestAlphaCompositing() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ALPHA_COMPOSITING_H_
