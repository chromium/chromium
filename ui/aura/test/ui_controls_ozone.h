// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_UI_CONTROLS_OZONE_H_
#define UI_AURA_TEST_UI_CONTROLS_OZONE_H_

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace aura::test {

class UIControlsOzone : public ui_controls::UIControlsAura {
 public:
  explicit UIControlsOzone(WindowTreeHost* host);
  UIControlsOzone(const UIControlsOzone&) = delete;
  UIControlsOzone& operator=(const UIControlsOzone&) = delete;
  ~UIControlsOzone() override;

 private:
  // ui_controls::UIControlsAura:
  bool SendKeyPress(gfx::NativeWindow window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override;
  bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                  ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt,
                                  bool command,
                                  base::OnceClosure closure) override;
  bool SendMouseMove(int screen_x,
                     int screen_y,
                     aura::Window* window_hint) override;
  bool SendMouseMoveNotifyWhenDone(int screen_x,
                                   int screen_y,
                                   base::OnceClosure closure,
                                   aura::Window* window_hint) override;
  bool SendMouseEvents(ui_controls::MouseButton type,
                       int button_state,
                       int accelerator_state,
                       aura::Window* window_hint) override;
  bool SendMouseEventsNotifyWhenDone(ui_controls::MouseButton type,
                                     int button_state,
                                     base::OnceClosure closure,
                                     int accelerator_state,
                                     aura::Window* window_hint) override;
  bool SendMouseClick(ui_controls::MouseButton type,
                      aura::Window* window_hint) override;
#if BUILDFLAG(IS_CHROMEOS)
  bool SendTouchEvents(int action, int id, int x, int y) override;
  bool SendTouchEventsNotifyWhenDone(int action,
                                     int id,
                                     int x,
                                     int y,
                                     base::OnceClosure task) override;
#endif

  // Use |optional_host| to specify the host.
  // When |optional_host| is not null, event will be sent to |optional_host|.
  // When |optional_host| is null, event will be sent to the default host.
  void SendEventToSink(ui::Event* event,
                       int64_t display_id,
                       base::OnceClosure closure,
                       WindowTreeHost* optional_host = nullptr);

  void PostKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    int flags,
                    int64_t display_id,
                    base::OnceClosure closure,
                    WindowTreeHost* optional_host = nullptr);

  void PostKeyEventTask(ui::EventType type,
                        ui::KeyboardCode key_code,
                        int flags,
                        int64_t display_id,
                        base::OnceClosure closure,
                        WindowTreeHost* optional_host);

  void PostMouseEvent(ui::EventType type,
                      const gfx::PointF& host_location,
                      int flags,
                      int changed_button_flags,
                      int64_t display_id,
                      base::OnceClosure closure,
                      aura::Window* window_hint);

  void PostMouseEventTask(ui::EventType type,
                          const gfx::PointF& host_location,
                          int flags,
                          int changed_button_flags,
                          int64_t display_id,
                          base::OnceClosure closure,
                          base::WeakPtr<WindowTreeHost> host_hint);

  void PostTouchEvent(ui::EventType type,
                      const gfx::PointF& host_location,
                      int id,
                      int64_t display_id,
                      base::OnceClosure closure);

  void PostTouchEventTask(ui::EventType type,
                          const gfx::PointF& host_location,
                          int id,
                          int64_t display_id,
                          base::OnceClosure closure);

  bool ScreenDIPToHostPixels(gfx::PointF* location, int64_t* display_id);

  // This is the default host used for events that are not scoped to a window.
  // Events scoped to a window always use the window's host.
  const raw_ptr<WindowTreeHost> host_;

  // Mask of the mouse buttons currently down. This is static as it needs to
  // track the state globally for all displays. A UIControlsOzone instance is
  // created for each display host.
  static unsigned button_down_mask_;
};

}  // namespace aura::test

#endif  // UI_AURA_TEST_UI_CONTROLS_OZONE_H_
