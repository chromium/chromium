// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_

#include <vector>

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class MockSurface;

// Manage wl_compositor object.
class TestCompositor : public GlobalObject {
 public:
  explicit TestCompositor(uint32_t intended_version);

  TestCompositor(const TestCompositor&) = delete;
  TestCompositor& operator=(const TestCompositor&) = delete;

  ~TestCompositor() override;

  void AddSurface(MockSurface* surface);
  uint32_t GetVersion() { return version_; }

 private:
  uint32_t version_;
  std::vector<MockSurface*> surfaces_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_
