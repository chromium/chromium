// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_

#include <vector>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class MockSurface;

// Manage wl_compositor object.
class TestCompositor : public GlobalObject {
 public:
  static constexpr uint32_t kVersion = 4;

  TestCompositor();
  ~TestCompositor() override;

  void AddSurface(MockSurface* surface);

 private:
  std::vector<MockSurface*> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositor);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_
