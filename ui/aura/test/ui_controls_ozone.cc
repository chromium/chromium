// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/ui_controls_ozone.h"

#include <tuple>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"

namespace aura {
namespace test {

// static
unsigned UIControlsOzone::button_down_mask_ = 0;

UIControlsOzone::UIControlsOzone(WindowTreeHost* host) : host_(host) {}

UIControlsOzone::~UIControlsOzone() = default;

bool UIControlsOzone::SendKeyEvents(gfx::NativeWindow window,
                                    ui::KeyboardCode key,
                                    int key_event_types,
                                    int accelerator_state) {
  return SendKeyEventsNotifyWhenDone(window, key, key_event_types,
                                     base::OnceClosure(), accelerator_state,
                                     ui_controls::KeyEventType::kKeyRelease);
}

bool UIControlsOzone::SendKeyEventsNotifyWhenDone(
    gfx::NativeWindow window,
    ui::KeyboardCode key,
    int key_event_types,
    base::OnceClosure closure,
    int accelerator_state,
    ui_controls::KeyEventType wait_for) {
  CHECK(wait_for == ui_controls::KeyEventType::kKeyPress ||
        wait_for == ui_controls::KeyEventType::kKeyRelease);
  // This doesn't time out if `window` is deleted before the key release events
  // are dispatched, so it's fine to ignore `wait_for` and always wait for key
  // release events.
  WindowTreeHost* optional_host = nullptr;
  // Send the key event to the window's host, which may not match |host_|.
  // This logic should probably exist for the non-aura path as well.
  // TODO(crbug.com/40144825) Support non-aura path.
#if defined(USE_AURA)
  if (window != nullptr && window->GetHost() != nullptr &&
      window->GetHost() != host_)
    optional_host = window->GetHost();
#endif

  bool has_press = key_event_types & ui_controls::kKeyPress;
  bool has_release = key_event_types & ui_controls::kKeyRelease;

  bool has_control = accelerator_state & ui_controls::kControl;
  bool has_shift = accelerator_state & ui_controls::kShift;
  bool has_command = accelerator_state & ui_controls::kCommand;
  bool has_alt = accelerator_state & ui_controls::kAlt;

  int flags = button_down_mask_;
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  if (has_press) {
    if (has_control) {
      flags |= ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_CONTROL, flags,
                   display_id, base::OnceClosure(), optional_host);
    }

    if (has_shift) {
      flags |= ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SHIFT, flags,
                   display_id, base::OnceClosure(), optional_host);
    }

    if (has_alt) {
      flags |= ui::EF_ALT_DOWN;
      PostKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_MENU, flags, display_id,
                   base::OnceClosure(), optional_host);
    }

    if (has_command) {
      flags |= ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN, flags, display_id,
                   base::OnceClosure(), optional_host);
    }

    PostKeyEvent(ui::EventType::kKeyPressed, key, flags, display_id,
                 has_release ? base::OnceClosure() : std::move(closure),
                 optional_host);
  }

  if (has_release) {
    PostKeyEvent(ui::EventType::kKeyReleased, key, flags, display_id,
                 (has_control || has_shift || has_alt || has_command)
                     ? base::OnceClosure()
                     : std::move(closure),
                 optional_host);

    if (has_alt) {
      flags &= ~ui::EF_ALT_DOWN;
      PostKeyEvent(
          ui::EventType::kKeyReleased, ui::VKEY_MENU, flags, display_id,
          (has_shift || has_control || has_command) ? base::OnceClosure()
                                                    : std::move(closure),
          optional_host);
    }

    if (has_shift) {
      flags &= ~ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_SHIFT, flags,
                   display_id,
                   (has_control || has_command) ? base::OnceClosure()
                                                : std::move(closure),
                   optional_host);
    }

    if (has_control) {
      flags &= ~ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_CONTROL, flags,
                   display_id,
                   has_command ? base::OnceClosure() : std::move(closure),
                   optional_host);
    }

    if (has_command) {
      flags &= ~ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN, flags,
                   display_id, std::move(closure), optional_host);
    }
  }

  return true;
}

bool UIControlsOzone::SendMouseMove(int screen_x, int screen_y) {
  return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
}

bool UIControlsOzone::SendMouseMoveNotifyWhenDone(int screen_x,
                                                  int screen_y,
                                                  base::OnceClosure closure) {
  gfx::PointF host_location(screen_x, screen_y);
  int64_t display_id = display::kInvalidDisplayId;
  if (!ScreenDIPToHostPixels(&host_location, &display_id))
    return false;
  ui::EventType event_type;

  if (button_down_mask_)
    event_type = ui::EventType::kMouseDragged;
  else
    event_type = ui::EventType::kMouseMoved;

  PostMouseEvent(event_type, host_location, button_down_mask_, 0, display_id,
                 std::move(closure));

  return true;
}

bool UIControlsOzone::SendMouseEvents(ui_controls::MouseButton type,
                                      int button_state,
                                      int accelerator_state) {
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state);
}

bool UIControlsOzone::SendMouseEventsNotifyWhenDone(
    ui_controls::MouseButton type,
    int button_state,
    base::OnceClosure closure,
    int accelerator_state) {
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
      NOTREACHED_IN_MIGRATION();
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
    PostMouseEvent(ui::EventType::kMousePressed, host_location,
                   button_down_mask_ | flag, changed_button_flag, display_id,
                   (button_state & ui_controls::UP) ? base::OnceClosure()
                                                    : std::move(closure));
  }
  if (button_state & ui_controls::UP) {
    button_down_mask_ &= ~flag;
    PostMouseEvent(ui::EventType::kMouseReleased, host_location,
                   button_down_mask_ | flag, changed_button_flag, display_id,
                   std::move(closure));
  }

  return true;
}

bool UIControlsOzone::SendMouseClick(ui_controls::MouseButton type) {
  return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN,
                         ui_controls::kNoAccelerator);
}

#if BUILDFLAG(IS_CHROMEOS)
bool UIControlsOzone::SendTouchEvents(int action, int id, int x, int y) {
  return SendTouchEventsNotifyWhenDone(action, id, x, y, base::OnceClosure());
}

bool UIControlsOzone::SendTouchEventsNotifyWhenDone(int action,
                                                    int id,
                                                    int x,
                                                    int y,
                                                    base::OnceClosure task) {
  DCHECK_NE(0, action);
  gfx::PointF host_location(x, y);
  int64_t display_id = display::kInvalidDisplayId;
  if (!ScreenDIPToHostPixels(&host_location, &display_id))
    return false;
  bool has_move = action & ui_controls::kTouchMove;
  bool has_release = action & ui_controls::kTouchRelease;
  if (action & ui_controls::kTouchPress) {
    PostTouchEvent(
        ui::EventType::kTouchPressed, host_location, id, display_id,
        (has_move || has_release) ? base::OnceClosure() : std::move(task));
  }
  if (has_move) {
    PostTouchEvent(ui::EventType::kTouchMoved, host_location, id, display_id,
                   has_release ? base::OnceClosure() : std::move(task));
  }
  if (has_release) {
    PostTouchEvent(ui::EventType::kTouchReleased, host_location, id, display_id,
                   std::move(task));
  }
  return true;
}
#endif

void UIControlsOzone::SendEventToSink(ui::Event* event,
                                      int64_t display_id,
                                      base::OnceClosure closure,
                                      WindowTreeHost* optional_host,
                                      bool post_task_after_dispatch) {
  // Post the task before processing the event. This is usually necessary in
  // case processing the event results in a nested message loop.
  if (closure && !post_task_after_dispatch) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }

  WindowTreeHost* host = optional_host ? optional_host : host_.get();
  ui::EventSourceTestApi event_source_test(host->GetEventSource());
  std::ignore = event_source_test.SendEventToSink(event);

  // It is sometimes necessary to post the task after processing the event.
  // This should only occur if it is known that the event does not enter any
  // nested message loops.
  if (closure && post_task_after_dispatch) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
}

void UIControlsOzone::PostKeyEvent(ui::EventType type,
                                   ui::KeyboardCode key_code,
                                   int flags,
                                   int64_t display_id,
                                   base::OnceClosure closure,
                                   WindowTreeHost* optional_host) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UIControlsOzone::PostKeyEventTask,
                                base::Unretained(this), type, key_code, flags,
                                display_id, std::move(closure), optional_host));
}

void UIControlsOzone::PostKeyEventTask(ui::EventType type,
                                       ui::KeyboardCode key_code,
                                       int flags,
                                       int64_t display_id,
                                       base::OnceClosure closure,
                                       WindowTreeHost* optional_host) {
  // Do not rewrite injected events. See crbug.com/136465.
  flags |= ui::EF_FINAL;

  ui::KeyEvent key_event(type, key_code, flags);
  if (type == ui::EventType::kKeyPressed) {
    // Set a property as if this is a key event not consumed by IME.
    // Ozone/X11+GTK IME works so already. Ozone/wayland IME relies on this
    // flag to work properly.
    ui::SetKeyboardImeFlags(&key_event, ui::kPropertyKeyboardImeIgnoredFlag);
  }
  SendEventToSink(&key_event, display_id, std::move(closure), optional_host);
}

void UIControlsOzone::PostMouseEvent(ui::EventType type,
                                     const gfx::PointF& host_location,
                                     int flags,
                                     int changed_button_flags,
                                     int64_t display_id,
                                     base::OnceClosure closure) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UIControlsOzone::PostMouseEventTask,
                     base::Unretained(this), type, host_location, flags,
                     changed_button_flags, display_id, std::move(closure)));
}

void UIControlsOzone::PostMouseEventTask(ui::EventType type,
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

  // For drag-ending left-mouse-button release events, the closure must be
  // posted after the event is processed to ensure zcr_ui_controls::
  // request_processed is sent after wl_data_source::dnd_finished.
  // TODO(crbug.com/41489982): Desired synchronization semantics should
  // be declared explicitly, not decided by test framework heuristics.
  bool post_task_after_dispatch =
      changed_button_flags == ui::EF_LEFT_MOUSE_BUTTON &&
      type == ui::EventType::kMouseReleased;
  SendEventToSink(&mouse_event2, display_id, std::move(closure), nullptr,
                  post_task_after_dispatch);
}

void UIControlsOzone::PostTouchEvent(ui::EventType type,
                                     const gfx::PointF& host_location,
                                     int id,
                                     int64_t display_id,
                                     base::OnceClosure closure) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UIControlsOzone::PostTouchEventTask,
                                base::Unretained(this), type, host_location, id,
                                display_id, std::move(closure)));
}

void UIControlsOzone::PostTouchEventTask(ui::EventType type,
                                         const gfx::PointF& host_location,
                                         int id,
                                         int64_t display_id,
                                         base::OnceClosure closure) {
  ui::PointerDetails details(ui::EventPointerType::kTouch, id, 1.0f, 1.0f,
                             0.0f);
  ui::TouchEvent touch_event(type, host_location, host_location,
                             ui::EventTimeForNow(), details);
  SendEventToSink(&touch_event, display_id, std::move(closure));
}

bool UIControlsOzone::ScreenDIPToHostPixels(gfx::PointF* location,
                                            int64_t* display_id) {
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

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsOzone(host);
}

}  // namespace test
}  // namespace aura
