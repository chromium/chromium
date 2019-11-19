// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

namespace aura {
namespace test {
namespace {

class UIControlsOzone : public ui_controls::UIControlsAura {
 public:
  UIControlsOzone(WindowTreeHost* host) : host_(host) {
  }
  ~UIControlsOzone() override = default;

 private:
  // ui_controls::UIControlsAura:
  bool SendKeyPress(gfx::NativeWindow window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                      base::OnceClosure());
  }
  bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                  ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt,
                                  bool command,
                                  base::OnceClosure closure) override {
    int flags = button_down_mask_;
    int64_t display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

    if (control) {
      flags |= ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, flags, display_id,
                   base::OnceClosure());
    }

    if (shift) {
      flags |= ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, flags, display_id,
                   base::OnceClosure());
    }

    if (alt) {
      flags |= ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_MENU, flags, display_id,
                   base::OnceClosure());
    }

    if (command) {
      flags |= ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LWIN, flags, display_id,
                   base::OnceClosure());
    }

    PostKeyEvent(ui::ET_KEY_PRESSED, key, flags, display_id,
                 base::OnceClosure());
    const bool has_modifier = control || shift || alt || command;
    // Pass the real closure to the last generated KeyEvent.
    PostKeyEvent(ui::ET_KEY_RELEASED, key, flags, display_id,
                 has_modifier ? base::OnceClosure() : std::move(closure));

    if (alt) {
      flags &= ~ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_MENU, flags, display_id,
                   (shift || control || command) ? base::OnceClosure()
                                                 : std::move(closure));
    }

    if (shift) {
      flags &= ~ui::EF_SHIFT_DOWN;
      PostKeyEvent(
          ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, flags, display_id,
          (control || command) ? base::OnceClosure() : std::move(closure));
    }

    if (control) {
      flags &= ~ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, flags, display_id,
                   command ? base::OnceClosure() : std::move(closure));
    }

    if (command) {
      flags &= ~ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_LWIN, flags, display_id,
                   std::move(closure));
    }

    return true;
  }

  bool SendMouseMove(long screen_x, long screen_y) override {
    return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
  }
  bool SendMouseMoveNotifyWhenDone(long screen_x,
                                   long screen_y,
                                   base::OnceClosure closure) override {
    gfx::PointF host_location(screen_x, screen_y);
    int64_t display_id = display::kInvalidDisplayId;
    if (!ScreenDIPToHostPixels(&host_location, &display_id))
      return false;
    ui::EventType event_type;

    if (button_down_mask_)
      event_type = ui::ET_MOUSE_DRAGGED;
    else
      event_type = ui::ET_MOUSE_MOVED;

    PostMouseEvent(event_type, host_location, button_down_mask_, 0, display_id,
                   std::move(closure));

    return true;
  }
  bool SendMouseEvents(ui_controls::MouseButton type,
                       int button_state,
                       int accelerator_state) override {
    return SendMouseEventsNotifyWhenDone(
        type, button_state, base::OnceClosure(), accelerator_state);
  }
  bool SendMouseEventsNotifyWhenDone(ui_controls::MouseButton type,
                                     int button_state,
                                     base::OnceClosure closure,
                                     int accelerator_state) override {
    gfx::PointF host_location(Env::GetInstance()->last_mouse_location());
    int64_t display_id = display::kInvalidDisplayId;
    if (!ScreenDIPToHostPixels(&host_location, &display_id))
      return false;

    int changed_button_flag = 0;

    switch (type) {
      case ui_controls::LEFT:
        changed_button_flag = ui::EF_LEFT_MOUSE_BUTTON;
        break;
      case ui_controls::MIDDLE:
        changed_button_flag = ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      case ui_controls::RIGHT:
        changed_button_flag = ui::EF_RIGHT_MOUSE_BUTTON;
        break;
      default:
        NOTREACHED();
        break;
    }

    // Process the accelerator key state.
    int flag = changed_button_flag;
    if (accelerator_state & ui_controls::kShift)
      flag |= ui::EF_SHIFT_DOWN;
    if (accelerator_state & ui_controls::kControl)
      flag |= ui::EF_CONTROL_DOWN;
    if (accelerator_state & ui_controls::kAlt)
      flag |= ui::EF_ALT_DOWN;
    if (accelerator_state & ui_controls::kCommand)
      flag |= ui::EF_COMMAND_DOWN;

    if (button_state & ui_controls::DOWN) {
      button_down_mask_ |= flag;
      // Pass the real closure to the last generated MouseEvent.
      PostMouseEvent(ui::ET_MOUSE_PRESSED, host_location,
                     button_down_mask_ | flag, changed_button_flag, display_id,
                     (button_state & ui_controls::UP) ? base::OnceClosure()
                                                      : std::move(closure));
    }
    if (button_state & ui_controls::UP) {
      button_down_mask_ &= ~flag;
      PostMouseEvent(ui::ET_MOUSE_RELEASED, host_location,
                     button_down_mask_ | flag, changed_button_flag, display_id,
                     std::move(closure));
    }

    return true;
  }
  bool SendMouseClick(ui_controls::MouseButton type) override {
    return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN,
                           ui_controls::kNoAccelerator);
  }
#if defined(OS_CHROMEOS)
  bool SendTouchEvents(int action, int id, int x, int y) override {
    return SendTouchEventsNotifyWhenDone(action, id, x, y, base::OnceClosure());
  }
  bool SendTouchEventsNotifyWhenDone(int action,
                                     int id,
                                     int x,
                                     int y,
                                     base::OnceClosure task) override {
    DCHECK_NE(0, action);
    gfx::PointF host_location(x, y);
    int64_t display_id = display::kInvalidDisplayId;
    if (!ScreenDIPToHostPixels(&host_location, &display_id))
      return false;
    bool has_move = action & ui_controls::MOVE;
    bool has_release = action & ui_controls::RELEASE;
    if (action & ui_controls::PRESS) {
      PostTouchEvent(
          ui::ET_TOUCH_PRESSED, host_location, id, display_id,
          (has_move || has_release) ? base::OnceClosure() : std::move(task));
    }
    if (has_move) {
      PostTouchEvent(ui::ET_TOUCH_MOVED, host_location, id, display_id,
                     has_release ? base::OnceClosure() : std::move(task));
    }
    if (has_release) {
      PostTouchEvent(ui::ET_TOUCH_RELEASED, host_location, id, display_id,
                     std::move(task));
    }
    return true;
  }
#endif

  void SendEventToSink(ui::Event* event,
                       int64_t display_id,
                       base::OnceClosure closure) {
    // Post the task before processing the event. This is necessary in case
    // processing the event results in a nested message loop.
    if (closure) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(closure));
    }

    ui::EventSourceTestApi event_source_test(host_->GetEventSource());
    ignore_result(event_source_test.SendEventToSink(event));
  }

  void PostKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    int flags,
                    int64_t display_id,
                    base::OnceClosure closure) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&UIControlsOzone::PostKeyEventTask,
                                  base::Unretained(this), type, key_code, flags,
                                  display_id, std::move(closure)));
  }

  void PostKeyEventTask(ui::EventType type,
                        ui::KeyboardCode key_code,
                        int flags,
                        int64_t display_id,
                        base::OnceClosure closure) {
    // Do not rewrite injected events. See crbug.com/136465.
    flags |= ui::EF_FINAL;

    ui::KeyEvent key_event(type, key_code, flags);
    SendEventToSink(&key_event, display_id, std::move(closure));
  }

  void PostMouseEvent(ui::EventType type,
                      const gfx::PointF& host_location,
                      int flags,
                      int changed_button_flags,
                      int64_t display_id,
                      base::OnceClosure closure) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UIControlsOzone::PostMouseEventTask,
                       base::Unretained(this), type, host_location, flags,
                       changed_button_flags, display_id, std::move(closure)));
  }

  void PostMouseEventTask(ui::EventType type,
                          const gfx::PointF& host_location,
                          int flags,
                          int changed_button_flags,
                          int64_t display_id,
                          base::OnceClosure closure) {
    ui::MouseEvent mouse_event(type, host_location, host_location,
                               ui::EventTimeForNow(), flags,
                               changed_button_flags);

    // This hack is necessary to set the repeat count for clicks.
    ui::MouseEvent mouse_event2(&mouse_event);

    SendEventToSink(&mouse_event2, display_id, std::move(closure));
  }

  void PostTouchEvent(ui::EventType type,
                      const gfx::PointF& host_location,
                      int id,
                      int64_t display_id,
                      base::OnceClosure closure) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&UIControlsOzone::PostTouchEventTask,
                                  base::Unretained(this), type, host_location,
                                  id, display_id, std::move(closure)));
  }

  void PostTouchEventTask(ui::EventType type,
                          const gfx::PointF& host_location,
                          int id,
                          int64_t display_id,
                          base::OnceClosure closure) {
    ui::PointerDetails details(ui::EventPointerType::POINTER_TYPE_TOUCH, id,
                               1.0f, 1.0f, 0.0f);
    ui::TouchEvent touch_event(type, host_location, host_location,
                               ui::EventTimeForNow(), details);
    SendEventToSink(&touch_event, display_id, std::move(closure));
  }

  bool ScreenDIPToHostPixels(gfx::PointF* location, int64_t* display_id) {
    // The location needs to be in display's coordinate.
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestPoint(
            gfx::ToFlooredPoint(*location));
    if (!display.is_valid()) {
      LOG(ERROR) << "Failed to find the display for " << location->ToString();
      return false;
    }
    *display_id = display.id();
    *location -= display.bounds().OffsetFromOrigin();
    location->Scale(display.device_scale_factor());
    return true;
  }

  WindowTreeHost* host_;

  // Mask of the mouse buttons currently down. This is static as it needs to
  // track the state globally for all displays. A UIControlsOzone instance is
  // created for each display host.
  static unsigned button_down_mask_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsOzone);
};

// static
unsigned UIControlsOzone::button_down_mask_ = 0;

}  // namespace

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsOzone(host);
}

}  // namespace test
}  // namespace aura
