// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/ui_controls_ozone.h"

#include <tuple>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event_utils.h"

namespace aura {
namespace test {

// static
unsigned UIControlsOzone::button_down_mask_ = 0;

UIControlsOzone::UIControlsOzone(WindowTreeHost* host) : host_(host) {}

UIControlsOzone::~UIControlsOzone() = default;

bool UIControlsOzone::SendKeyPress(gfx::NativeWindow window,
                                   ui::KeyboardCode key,
                                   bool control,
                                   bool shift,
                                   bool alt,
                                   bool command) {
  return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                    base::OnceClosure());
}

bool UIControlsOzone::SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                                 ui::KeyboardCode key,
                                                 bool control,
                                                 bool shift,
                                                 bool alt,
                                                 bool command,
                                                 base::OnceClosure closure) {
  WindowTreeHost* optional_host = nullptr;
  // Send the key event to the window's host, which may not match |host_|.
  // This logic should probably exist for the non-aura path as well.
  // TODO(https://crbug.com/1116649) Support non-aura path.
#if defined(USE_AURA)
  if (window != nullptr && window->GetHost() != nullptr &&
      window->GetHost() != host_)
    optional_host = window->GetHost();
#endif

  int flags = button_down_mask_;
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  if (control) {
    flags |= ui::EF_CONTROL_DOWN;
    PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, flags, display_id,
                 base::OnceClosure(), optional_host);
  }

  if (shift) {
    flags |= ui::EF_SHIFT_DOWN;
    PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, flags, display_id,
                 base::OnceClosure(), optional_host);
  }

  if (alt) {
    flags |= ui::EF_ALT_DOWN;
    PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_MENU, flags, display_id,
                 base::OnceClosure(), optional_host);
  }

  if (command) {
    flags |= ui::EF_COMMAND_DOWN;
    PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LWIN, flags, display_id,
                 base::OnceClosure(), optional_host);
  }

  PostKeyEvent(ui::ET_KEY_PRESSED, key, flags, display_id, base::OnceClosure(),
               optional_host);
  const bool has_modifier = control || shift || alt || command;
  // Pass the real closure to the last generated KeyEvent.
  PostKeyEvent(ui::ET_KEY_RELEASED, key, flags, display_id,
               has_modifier ? base::OnceClosure() : std::move(closure),
               optional_host);

  if (alt) {
    flags &= ~ui::EF_ALT_DOWN;
    PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_MENU, flags, display_id,
                 (shift || control || command) ? base::OnceClosure()
                                               : std::move(closure),
                 optional_host);
  }

  if (shift) {
    flags &= ~ui::EF_SHIFT_DOWN;
    PostKeyEvent(
        ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, flags, display_id,
        (control || command) ? base::OnceClosure() : std::move(closure),
        optional_host);
  }

  if (control) {
    flags &= ~ui::EF_CONTROL_DOWN;
    PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, flags, display_id,
                 command ? base::OnceClosure() : std::move(closure),
                 optional_host);
  }

  if (command) {
    flags &= ~ui::EF_COMMAND_DOWN;
    PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_LWIN, flags, display_id,
                 std::move(closure), optional_host);
  }

  return true;
}

bool UIControlsOzone::SendMouseMove(int screen_x,
                                    int screen_y,
                                    aura::Window* window_hint) {
  return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure(),
                                     window_hint);
}

bool UIControlsOzone::SendMouseMoveNotifyWhenDone(int screen_x,
                                                  int screen_y,
                                                  base::OnceClosure closure,
                                                  aura::Window* window_hint) {
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
                 std::move(closure), window_hint);

  return true;
}

bool UIControlsOzone::SendMouseEvents(ui_controls::MouseButton type,
                                      int button_state,
                                      int accelerator_state,
                                      aura::Window* window_hint) {
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state, window_hint);
}

bool UIControlsOzone::SendMouseEventsNotifyWhenDone(
    ui_controls::MouseButton type,
    int button_state,
    base::OnceClosure closure,
    int accelerator_state,
    aura::Window* window_hint) {
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
                                                    : std::move(closure),
                   window_hint);
  }
  if (button_state & ui_controls::UP) {
    button_down_mask_ &= ~flag;
    PostMouseEvent(ui::ET_MOUSE_RELEASED, host_location,
                   button_down_mask_ | flag, changed_button_flag, display_id,
                   std::move(closure), window_hint);
  }

  return true;
}

bool UIControlsOzone::SendMouseClick(ui_controls::MouseButton type,
                                     aura::Window* window_hint) {
  return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN,
                         ui_controls::kNoAccelerator, window_hint);
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

void UIControlsOzone::SendEventToSink(ui::Event* event,
                                      int64_t display_id,
                                      base::OnceClosure closure,
                                      WindowTreeHost* optional_host) {
  // Post the task before processing the event. This is necessary in case
  // processing the event results in a nested message loop.
  if (closure) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
  WindowTreeHost* host = optional_host ? optional_host : host_.get();
  ui::EventSourceTestApi event_source_test(host->GetEventSource());
  std::ignore = event_source_test.SendEventToSink(event);
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
  if (type == ui::ET_KEY_PRESSED) {
    // Set a property as if this is a key event not consumed by IME.
    // Ozone/X11+GTK IME works so already. Ozone/wayland IME relies on this
    // flag to work properly.
    key_event.SetProperties({{
        ui::kPropertyKeyboardImeFlag,
        std::vector<uint8_t>{ui::kPropertyKeyboardImeIgnoredFlag},
    }});
  }
  SendEventToSink(&key_event, display_id, std::move(closure), optional_host);
}

void UIControlsOzone::PostMouseEvent(ui::EventType type,
                                     const gfx::PointF& host_location,
                                     int flags,
                                     int changed_button_flags,
                                     int64_t display_id,
                                     base::OnceClosure closure,
                                     aura::Window* window_hint) {
  base::WeakPtr<WindowTreeHost> host_hint =
      (window_hint && window_hint->GetHost())
          ? window_hint->GetHost()->GetWeakPtr()
          : nullptr;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UIControlsOzone::PostMouseEventTask,
                                base::Unretained(this), type, host_location,
                                flags, changed_button_flags, display_id,
                                std::move(closure), host_hint));
}

void UIControlsOzone::PostMouseEventTask(
    ui::EventType type,
    const gfx::PointF& host_location,
    int flags,
    int changed_button_flags,
    int64_t display_id,
    base::OnceClosure closure,
    base::WeakPtr<WindowTreeHost> host_hint) {
  ui::MouseEvent mouse_event(type, host_location, host_location,
                             ui::EventTimeForNow(), flags,
                             changed_button_flags);

  // This hack is necessary to set the repeat count for clicks.
  ui::MouseEvent mouse_event2(&mouse_event);

  SendEventToSink(&mouse_event2, display_id, std::move(closure),
                  host_hint.get());
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
