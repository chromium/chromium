// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OVERLAY_PRIORITIZER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OVERLAY_PRIORITIZER_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manage overlay_prioritizer object.
class TestOverlayPrioritizer : public GlobalObject {
 public:
  TestOverlayPrioritizer();
  ~TestOverlayPrioritizer() override;
  TestOverlayPrioritizer(const TestOverlayPrioritizer& rhs) = delete;
  TestOverlayPrioritizer& operator=(const TestOverlayPrioritizer& rhs) = delete;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OVERLAY_PRIORITIZER_H_
