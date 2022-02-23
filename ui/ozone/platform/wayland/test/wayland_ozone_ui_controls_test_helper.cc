// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/wayland_ozone_ui_controls_test_helper.h"

#include <linux/input.h>

#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/ozone/platform/wayland/emulate/wayland_input_emulate.h"

namespace wl {

namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UP;

class WaylandGlobalEventWaiter : public WaylandInputEmulate::Observer {
 public:
  enum class WaylandEventType {
    kMotion,
    kButton,
    kKey,
    kUnknown,
  };

  static WaylandGlobalEventWaiter* Create(WaylandEventType event_type,
                                          int button,
                                          bool pressed,
                                          base::OnceClosure closure,
                                          WaylandInputEmulate* emulate) {
    DCHECK_NE(event_type, WaylandEventType::kUnknown);
    return closure.is_null()
               ? nullptr
               : new WaylandGlobalEventWaiter(event_type, button, pressed,
                                              std::move(closure), emulate);
  }

  static WaylandGlobalEventWaiter* Create(WaylandEventType event_type,
                                          const gfx::Point& screen_point,
                                          base::OnceClosure closure,
                                          WaylandInputEmulate* emulate) {
    DCHECK_NE(event_type, WaylandEventType::kUnknown);
    return closure.is_null()
               ? nullptr
               : new WaylandGlobalEventWaiter(event_type, screen_point,
                                              std::move(closure), emulate);
  }

 private:
  WaylandGlobalEventWaiter(WaylandEventType event_type,
                           const gfx::Point& screen_point,
                           base::OnceClosure closure,
                           WaylandInputEmulate* emulate)
      : event_type_(event_type),
        closure_(std::move(closure)),
        emulate_(emulate),
        screen_point_(screen_point) {
    Initialize();
  }

  WaylandGlobalEventWaiter(WaylandEventType event_type,
                           int button,
                           bool pressed,
                           base::OnceClosure closure,
                           WaylandInputEmulate* emulate)
      : event_type_(event_type),
        closure_(std::move(closure)),
        emulate_(emulate),
        button_or_key_(button),
        pressed_(pressed) {
    Initialize();
  }

  ~WaylandGlobalEventWaiter() override { emulate_->RemoveObserver(this); }

  void Initialize() {
    DCHECK_NE(event_type_, WaylandEventType::kUnknown);
    DCHECK(!closure_.is_null());
    emulate_->AddObserver(this);
  }

  void OnPointerMotionGlobal(const gfx::Point& screen_position) override {
    if (event_type_ == WaylandEventType::kMotion) {
      ExecuteClosure();
    }
  }

  void OnPointerButtonGlobal(int32_t button, bool pressed) override {
    if (event_type_ == WaylandEventType::kButton && button_or_key_ == button &&
        pressed == pressed_) {
      ExecuteClosure();
    }
  }

  void OnKeyboardKey(int32_t key, bool pressed) override {
    if (event_type_ == WaylandEventType::kKey && button_or_key_ == key &&
        pressed == pressed_) {
      ExecuteClosure();
    }
  }

  void ExecuteClosure() {
    DCHECK(!closure_.is_null());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(closure_));
    delete this;
  }

  // Expected event type.
  WaylandEventType event_type_ = WaylandEventType::kUnknown;

  base::OnceClosure closure_;

  WaylandInputEmulate* const emulate_;

  // Expected pointer location on screen.
  gfx::Point screen_point_;

  // Expected button or key.
  int button_or_key_ = -1;

  // Expected key or button to be pressed or depressed.
  bool pressed_ = false;
};

}  // namespace

WaylandOzoneUIControlsTestHelper::WaylandOzoneUIControlsTestHelper()
    : input_emulate_(std::make_unique<WaylandInputEmulate>()) {}

WaylandOzoneUIControlsTestHelper::~WaylandOzoneUIControlsTestHelper() = default;

unsigned WaylandOzoneUIControlsTestHelper::ButtonDownMask() const {
  return button_down_mask_;
}

void WaylandOzoneUIControlsTestHelper::SendKeyPressEvent(
    gfx::AcceleratedWidget widget,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    base::OnceClosure closure) {
  SendKeyPressInternal(widget, key, control, shift, alt, command, {},
                       true /* key press */);
  SendKeyPressInternal(widget, key, control, shift, alt, command,
                       std::move(closure), false /* key release */);
}

void WaylandOzoneUIControlsTestHelper::SendMouseMotionNotifyEvent(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_root_loc,
    base::OnceClosure closure) {
  WaylandGlobalEventWaiter::Create(
      WaylandGlobalEventWaiter::WaylandEventType::kMotion, mouse_loc,
      std::move(closure), input_emulate_.get());

  input_emulate_->EmulatePointerMotion(widget, mouse_loc);
}

void WaylandOzoneUIControlsTestHelper::SendMouseEvent(
    gfx::AcceleratedWidget widget,
    ui_controls::MouseButton type,
    int button_state,
    int accelerator_state,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_root_loc,
    base::OnceClosure closure) {
  uint32_t changed_button = 0;
  switch (type) {
    case LEFT:
      changed_button = BTN_LEFT;
      break;
    case MIDDLE:
      changed_button = BTN_MIDDLE;
      break;
    case RIGHT:
      changed_button = BTN_RIGHT;
      break;
    default:
      NOTREACHED();
  }

  // Press accelerator keys.
  if (accelerator_state) {
    SendKeyPressInternal(widget, ui::KeyboardCode::VKEY_UNKNOWN,
                         accelerator_state & ui_controls::kControl,
                         accelerator_state & ui_controls::kShift,
                         accelerator_state & ui_controls::kAlt,
                         accelerator_state & ui_controls::kCommand, {}, true);
  }

  SendMouseMotionNotifyEvent(widget, mouse_loc, mouse_root_loc, {});

  WaylandGlobalEventWaiter::Create(
      WaylandGlobalEventWaiter::WaylandEventType::kButton, changed_button,
      button_state & DOWN, std::move(closure), input_emulate_.get());

  if (button_state & DOWN) {
    input_emulate_->EmulatePointerButton(
        widget, ui::EventType::ET_MOUSE_PRESSED, changed_button);
    button_down_mask_ |= changed_button;
  }
  if (button_state & UP) {
    input_emulate_->EmulatePointerButton(
        widget, ui::EventType::ET_MOUSE_RELEASED, changed_button);
    button_down_mask_ = (button_down_mask_ | changed_button) ^ changed_button;
  }

  // Depress accelerator keys.
  if (accelerator_state) {
    SendKeyPressInternal(widget, ui::KeyboardCode::VKEY_UNKNOWN,
                         accelerator_state & ui_controls::kControl,
                         accelerator_state & ui_controls::kShift,
                         accelerator_state & ui_controls::kAlt,
                         accelerator_state & ui_controls::kCommand, {}, false);
  }
}

void WaylandOzoneUIControlsTestHelper::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  NOTREACHED();
}

bool WaylandOzoneUIControlsTestHelper::MustUseUiControlsForMoveCursorTo() {
  return true;
}

void WaylandOzoneUIControlsTestHelper::SendKeyPressInternal(
    gfx::AcceleratedWidget widget,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    base::OnceClosure closure,
    bool press_key) {
  auto dom_code = UsLayoutKeyboardCodeToDomCode(key);

  WaylandGlobalEventWaiter::Create(
      WaylandGlobalEventWaiter::WaylandEventType::kKey,
      ui::KeycodeConverter::DomCodeToEvdevCode(dom_code), press_key,
      std::move(closure), input_emulate_.get());

  if (press_key) {
    if (control)
      DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED,
                       ui::DomCode::CONTROL_LEFT);

    if (shift)
      DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED,
                       ui::DomCode::SHIFT_LEFT);

    if (alt)
      DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED,
                       ui::DomCode::ALT_LEFT);

    DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED, dom_code);
  } else {
    DispatchKeyPress(widget, ui::EventType::ET_KEY_RELEASED, dom_code);

    if (alt)
      DispatchKeyPress(widget, ui::EventType::ET_KEY_RELEASED,
                       ui::DomCode::ALT_LEFT);

    if (shift)
      DispatchKeyPress(widget, ui::EventType::ET_KEY_RELEASED,
                       ui::DomCode::SHIFT_LEFT);

    if (control)
      DispatchKeyPress(widget, ui::EventType::ET_KEY_RELEASED,
                       ui::DomCode::CONTROL_LEFT);
  }
}

void WaylandOzoneUIControlsTestHelper::DispatchKeyPress(
    gfx::AcceleratedWidget widget,
    ui::EventType event_type,
    ui::DomCode dom_code) {
  input_emulate_->EmulateKeyboardKey(widget, event_type, dom_code);
}

}  // namespace wl

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland() {
  return new wl::WaylandOzoneUIControlsTestHelper();
}

}  // namespace ui
