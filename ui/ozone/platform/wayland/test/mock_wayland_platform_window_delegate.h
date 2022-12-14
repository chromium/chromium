// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_

#include "base/memory/raw_ptr.h"
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
  int64_t InsertSequencePoint() override;

  int64_t viz_seq() const { return viz_seq_; }

 private:
  raw_ptr<WaylandWindow> wayland_window_ = nullptr;

  // |viz_seq_| is used to save an incrementing sequence point on each
  // call to InsertSequencePoint. Test code can check this value to know
  // what sequence point is required to advance to the latest state.
  int64_t viz_seq_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_
