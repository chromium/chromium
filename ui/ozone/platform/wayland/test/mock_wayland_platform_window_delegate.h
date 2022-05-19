// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_

#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {
class WaylandConnection;
class WaylandWindow;
struct PlatformWindowInitProperties;

class MockWaylandPlatformWindowDelegate : public MockPlatformWindowDelegate {
 public:
  MockWaylandPlatformWindowDelegate() = default;
  MockWaylandPlatformWindowDelegate(const MockWaylandPlatformWindowDelegate&) =
      delete;
  MockWaylandPlatformWindowDelegate operator=(
      const MockWaylandPlatformWindowDelegate&) = delete;
  ~MockWaylandPlatformWindowDelegate() override = default;

  std::unique_ptr<WaylandWindow> CreateWaylandWindow(
      WaylandConnection* connection,
      PlatformWindowInitProperties properties,
      bool update_visual_size_immediately = false,
      bool apply_pending_state_on_update_visual_size = false);

  // MockPlatformWindowDelegate:
  gfx::Rect ConvertRectToPixels(const gfx::Rect& rect_in_dp) const override;
  gfx::Rect ConvertRectToDIP(const gfx::Rect& rect_in_pixels) const override;

 private:
  WaylandWindow* wayland_window_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_
