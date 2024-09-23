// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/test/ui_controls_system_input_injector.h"

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

namespace {

// An implementation of UIControlsAura injecting evdev level inputs.
class UIControlsSystemInputInjector : public ui_controls::UIControlsAura {
 public:
  UIControlsSystemInputInjector() = default;

  UIControlsSystemInputInjector(const UIControlsSystemInputInjector&) = delete;
  UIControlsSystemInputInjector& operator=(
      const UIControlsSystemInputInjector&) = delete;

  ~UIControlsSystemInputInjector() override = default;

  bool SendKeyEvents(gfx::NativeWindow window,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state) override;

  bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                   ui::KeyboardCode key,
                                   int key_event_types,
                                   base::OnceClosure closure,
                                   int accelerator_state,
                                   ui_controls::KeyEventType wait_for) override;

  bool SendMouseMove(int screen_x, int screen_y) override;

  bool SendMouseMoveNotifyWhenDone(int screen_x,
                                   int screen_y,
                                   base::OnceClosure closure) override;

  bool SendMouseEvents(ui_controls::MouseButton type,
                       int button_state,
                       int accelerator_state) override;

  bool SendMouseEventsNotifyWhenDone(ui_controls::MouseButton type,
                                     int button_state,
                                     base::OnceClosure closure,
                                     int accelerator_state) override;

  bool SendMouseClick(ui_controls::MouseButton type) override;

#if BUILDFLAG(IS_CHROMEOS)
  bool SendTouchEvents(int action, int id, int x, int y) override {
    NOTIMPLEMENTED();
    return false;
  }
  bool SendTouchEventsNotifyWhenDone(int action,
                                     int id,
                                     int x,
                                     int y,
                                     base::OnceClosure task) override {
    NOTIMPLEMENTED();
    return false;
  }
#endif

  bool ScreenToHostCoord(gfx::PointF* location);

  void InjectAcceleratorEvents(int accelerator_state, bool down);

  void EnsureInputInjector();

 private:
  std::unique_ptr<ui::SystemInputInjector> input_injector_;
  scoped_refptr<base::SingleThreadTaskRunner> user_input_task_runner_;
};

bool UIControlsSystemInputInjector::SendKeyEvents(gfx::NativeWindow window,
                                                  ui::KeyboardCode key,
                                                  int key_event_types,
                                                  int accelerator_state) {
  return SendKeyEventsNotifyWhenDone(window, key, key_event_types,
                                     base::OnceClosure(), accelerator_state,
                                     ui_controls::KeyEventType::kKeyRelease);
}

bool UIControlsSystemInputInjector::SendKeyEventsNotifyWhenDone(
    gfx::NativeWindow window,
    ui::KeyboardCode key,
    int key_event_types,
    base::OnceClosure closure,
    int accelerator_state,
    ui_controls::KeyEventType wait_for) {
  CHECK(wait_for == ui_controls::KeyEventType::kKeyPress ||
        wait_for == ui_controls::KeyEventType::kKeyRelease);

  const bool has_press = key_event_types & ui_controls::kKeyPress;
  const bool has_release = key_event_types & ui_controls::kKeyRelease;

  const ui::DomCode key_dom = ui::UsLayoutKeyboardCodeToDomCode(key);

  EnsureInputInjector();
  if (has_press) {
    InjectAcceleratorEvents(accelerator_state, true);
    input_injector_->InjectKeyEvent(key_dom, true,
                                    /*suppress_auto_repeat=*/true);
  }

  if (has_release) {
    input_injector_->InjectKeyEvent(key_dom, false,
                                    /*suppress_auto_repeat=*/true);
    InjectAcceleratorEvents(accelerator_state, false);
  }

  if (closure) {
    user_input_task_runner_->PostTask(FROM_HERE, std::move(closure));
  }

  return true;
}

bool UIControlsSystemInputInjector::SendMouseMove(int screen_x, int screen_y) {
  return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
}

bool UIControlsSystemInputInjector::SendMouseMoveNotifyWhenDone(
    int screen_x,
    int screen_y,
    base::OnceClosure closure) {
  gfx::PointF host_location(screen_x, screen_y);
  if (!ScreenToHostCoord(&host_location)) {
    return false;
  }

  EnsureInputInjector();
  input_injector_->MoveCursorTo(host_location);
  if (closure) {
    user_input_task_runner_->PostTask(FROM_HERE, std::move(closure));
  }

  return true;
}

bool UIControlsSystemInputInjector::SendMouseEvents(
    ui_controls::MouseButton type,
    int button_state,
    int accelerator_state) {
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state);
}

bool UIControlsSystemInputInjector::SendMouseEventsNotifyWhenDone(
    ui_controls::MouseButton type,
    int button_state,
    base::OnceClosure closure,
    int accelerator_state) {
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

  const bool has_press = button_state & ui_controls::DOWN;
  const bool has_release = button_state & ui_controls::UP;

  EnsureInputInjector();
  if (has_press) {
    InjectAcceleratorEvents(accelerator_state, true);
    input_injector_->InjectMouseButton(changed_button_flag, true);
  }

  if (has_release) {
    input_injector_->InjectMouseButton(changed_button_flag, false);
    InjectAcceleratorEvents(accelerator_state, false);
  }

  if (closure) {
    user_input_task_runner_->PostTask(FROM_HERE, std::move(closure));
  }

  return true;
}

bool UIControlsSystemInputInjector::SendMouseClick(
    ui_controls::MouseButton type) {
  return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN,
                         ui_controls::kNoAccelerator);
}

bool UIControlsSystemInputInjector::ScreenToHostCoord(gfx::PointF* location) {
  // The location needs to be in display's coordinate.
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(
          gfx::ToFlooredPoint(*location));
  if (!display.is_valid()) {
    LOG(ERROR) << "Failed to find the display for " << location->ToString();
    return false;
  }
  *location -= display.bounds().OffsetFromOrigin();
  location->Scale(display.device_scale_factor());
  return true;
}

void UIControlsSystemInputInjector::InjectAcceleratorEvents(
    int accelerator_state,
    bool down) {
  const bool has_control = accelerator_state & ui_controls::kControl;
  const bool has_shift = accelerator_state & ui_controls::kShift;
  const bool has_command = accelerator_state & ui_controls::kCommand;
  const bool has_alt = accelerator_state & ui_controls::kAlt;

  if (has_control) {
    input_injector_->InjectKeyEvent(ui::DomCode::CONTROL_LEFT, down,
                                    /*suppress_auto_repeat=*/true);
  }
  if (has_shift) {
    input_injector_->InjectKeyEvent(ui::DomCode::SHIFT_LEFT, down,
                                    /*suppress_auto_repeat=*/true);
  }

  if (has_alt) {
    input_injector_->InjectKeyEvent(ui::DomCode::ALT_LEFT, down,
                                    /*suppress_auto_repeat=*/true);
  }

  if (has_command) {
    input_injector_->InjectKeyEvent(ui::DomCode::META_LEFT, down,
                                    /*suppress_auto_repeat=*/true);
  }
}

void UIControlsSystemInputInjector::EnsureInputInjector() {
  if (!input_injector_) {
    input_injector_ =
        ui::OzonePlatform::GetInstance()->CreateSystemInputInjector();
    // SystemInputInjector posts tasks to the UI thread, so we also need
    // to post closure to it.
    user_input_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }
}

}  // namespace

namespace ui::test {

void EnableUIControlsSystemInputInjector() {
  ui_controls::InstallUIControlsAura(new UIControlsSystemInputInjector());
}

}  // namespace ui::test
