// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WESTON_TEST_OZONE_UI_CONTROLS_TEST_HELPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WESTON_TEST_OZONE_UI_CONTROLS_TEST_HELPER_H_

#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/ozone_ui_controls_test_helper.h"

namespace wl {

class WestonTestInputEmulate;

class WestonTestOzoneUIControlsTestHelper
    : public ui::OzoneUIControlsTestHelper {
 public:
  WestonTestOzoneUIControlsTestHelper();
  WestonTestOzoneUIControlsTestHelper(
      const WestonTestOzoneUIControlsTestHelper&) = delete;
  WestonTestOzoneUIControlsTestHelper& operator=(
      const WestonTestOzoneUIControlsTestHelper&) = delete;
  ~WestonTestOzoneUIControlsTestHelper() override;

  // OzoneUIControlsTestHelper:
  bool SupportsScreenCoordinates() const override;
  unsigned ButtonDownMask() const override;
  void SendKeyEvents(gfx::AcceleratedWidget widget,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state,
                     base::OnceClosure closure) override;
  void SendMouseMotionNotifyEvent(gfx::AcceleratedWidget widget,
                                  const gfx::Point& mouse_loc,
                                  const gfx::Point& mouse_root_loc,
                                  base::OnceClosure closure) override;
  void SendMouseEvent(gfx::AcceleratedWidget widget,
                      ui_controls::MouseButton type,
                      int button_state,
                      int accelerator_state,
                      const gfx::Point& mouse_loc,
                      const gfx::Point& mouse_root_loc,
                      base::OnceClosure closure) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SendTouchEvent(gfx::AcceleratedWidget widget,
                      int action,
                      int id,
                      const gfx::Point& touch_loc,
                      base::OnceClosure closure) override;
#endif
  void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure) override;
  bool MustUseUiControlsForMoveCursorTo() override;

 private:
  // Sends either press or release key based |press_key| value.
  void SendKeyPressInternal(gfx::AcceleratedWidget widget,
                            ui::KeyboardCode key,
                            int accelerator_state,
                            base::OnceClosure closure,
                            bool press_key);
  void DispatchKeyPress(gfx::AcceleratedWidget widget,
                        ui::EventType event_type,
                        ui::DomCode key);

  unsigned button_down_mask_ = 0;

  std::unique_ptr<WestonTestInputEmulate> input_emulate_;
};

}  // namespace wl

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WESTON_TEST_OZONE_UI_CONTROLS_TEST_HELPER_H_
