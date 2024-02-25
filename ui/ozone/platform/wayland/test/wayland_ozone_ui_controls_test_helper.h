// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_OZONE_UI_CONTROLS_TEST_HELPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_OZONE_UI_CONTROLS_TEST_HELPER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/emulate/wayland_input_emulate.h"
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

  // OzoneUIControlsTestHelper:
  void Reset() override;
  bool SupportsScreenCoordinates() const override;
  unsigned ButtonDownMask() const override;
  void SendKeyEvents(gfx::AcceleratedWidget widget,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state,
                     base::OnceClosure closure) override;
  void SendMouseMotionNotifyEvent(gfx::AcceleratedWidget widget,
                                  const gfx::Point& mouse_loc,
                                  const gfx::Point& mouse_loc_in_screen,
                                  base::OnceClosure closure) override;
  void SendMouseEvent(gfx::AcceleratedWidget widget,
                      ui_controls::MouseButton type,
                      int button_state,
                      int accelerator_state,
                      const gfx::Point& mouse_loc,
                      const gfx::Point& mouse_loc_in_screen,
                      base::OnceClosure closure) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SendTouchEvent(gfx::AcceleratedWidget widget,
                      int action,
                      int id,
                      const gfx::Point& touch_loc,
                      base::OnceClosure closure) override;
  void UpdateDisplay(const std::string& display_specs,
                     base::OnceClosure closure) override;
#endif
  void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure) override;
  bool MustUseUiControlsForMoveCursorTo() override;
#if BUILDFLAG(IS_LINUX)
  void ForceUseScreenCoordinatesOnce() override;
#endif

 private:
  void RequestProcessed(uint32_t request_id);

  // Stores the closures to be executed when the request with the matching ID
  // has been processed.
  base::flat_map<uint32_t, base::OnceClosure> pending_closures_;

  std::unique_ptr<WaylandInputEmulate> input_emulate_;

  base::WeakPtrFactory<WaylandOzoneUIControlsTestHelper> weak_factory_{this};
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_WAYLAND_OZONE_UI_CONTROLS_TEST_HELPER_H_
