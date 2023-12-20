// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_

#include <cstdint>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class MockSurface;

// Manage wl_compositor object.
class TestCompositor : public GlobalObject {
 public:
  enum class Version : uint32_t {
    kV3 = 3,
    kV4 = 4,
  };
  explicit TestCompositor(Version intended_version);

  TestCompositor(const TestCompositor&) = delete;
  TestCompositor& operator=(const TestCompositor&) = delete;

  ~TestCompositor() override;

  void AddSurface(MockSurface* surface);
  Version GetVersion() { return version_; }

 private:
  Version version_;
  std::vector<raw_ptr<MockSurface, VectorExperimental>> surfaces_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_COMPOSITOR_H_
