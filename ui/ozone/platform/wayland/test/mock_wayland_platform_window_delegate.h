// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {
class WaylandConnection;
class WaylandWindow;
struct PlatformWindowInitProperties;

class MockWaylandPlatformWindowDelegate : public MockPlatformWindowDelegate {
 public:
  MockWaylandPlatformWindowDelegate();
  MockWaylandPlatformWindowDelegate(const MockWaylandPlatformWindowDelegate&) =
      delete;
  MockWaylandPlatformWindowDelegate operator=(
      const MockWaylandPlatformWindowDelegate&) = delete;
  ~MockWaylandPlatformWindowDelegate() override;

  std::unique_ptr<WaylandWindow> CreateWaylandWindow(
      WaylandConnection* connection,
      PlatformWindowInitProperties properties);

  // MockPlatformWindowDelegate:
  gfx::Rect ConvertRectToPixels(const gfx::Rect& rect_in_dp) const override;
  gfx::Rect ConvertRectToDIP(const gfx::Rect& rect_in_pixels) const override;
  int64_t OnStateUpdate(const PlatformWindowDelegate::State& old,
                        const PlatformWindowDelegate::State& latest) override;

  int64_t viz_seq() const { return viz_seq_; }

  // Callback called during OnStateUpdate. This can be used to simulate
  // re-entrant client initiated requests.
  void set_on_state_update_callback(base::RepeatingClosure cb) {
    on_state_update_callback_ = cb;
  }

 private:
  raw_ptr<WaylandWindow, AcrossTasksDanglingUntriaged> wayland_window_ =
      nullptr;

  // |viz_seq_| is used to save an incrementing sequence point on each
  // call to InsertSequencePoint. Test code can check this value to know
  // what sequence point is required to advance to the latest state.
  int64_t viz_seq_ = 0;

  base::RepeatingClosure on_state_update_callback_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_WAYLAND_PLATFORM_WINDOW_DELEGATE_H_
