// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_OZONE_UI_CONTROLS_TEST_HELPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_OZONE_UI_CONTROLS_TEST_HELPER_H_

#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/ozone_ui_controls_test_helper.h"

namespace wl {

class WaylandInputEmulate;

class WaylandOzoneUIControlsTestHelper : public ui::OzoneUIControlsTestHelper {
 public:
  WaylandOzoneUIControlsTestHelper();
  WaylandOzoneUIControlsTestHelper(const WaylandOzoneUIControlsTestHelper&) =
      delete;
  WaylandOzoneUIControlsTestHelper& operator=(
      const WaylandOzoneUIControlsTestHelper&) = delete;
  ~WaylandOzoneUIControlsTestHelper() override;

  unsigned ButtonDownMask() const override;
  void SendKeyPressEvent(gfx::AcceleratedWidget widget,
                         ui::KeyboardCode key,
                         bool control,
                         bool shift,
                         bool alt,
                         bool command,
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
                            bool control,
                            bool shift,
                            bool alt,
                            bool command,
                            base::OnceClosure closure,
                            bool press_key);
  void DispatchKeyPress(gfx::AcceleratedWidget widget,
                        ui::EventType event_type,
                        ui::DomCode key);

  unsigned button_down_mask_ = 0;

  std::unique_ptr<WaylandInputEmulate> input_emulate_;
};

}  // namespace wl

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_OZONE_UI_CONTROLS_TEST_HELPER_H_
