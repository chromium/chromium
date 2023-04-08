// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/weston_test_ozone_ui_controls_test_helper.h"
#include "base/memory/raw_ptr.h"

#include <linux/input.h>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/ozone/platform/wayland/emulate/weston_test_input_emulate.h"
#include "ui/ozone/platform/wayland/host/proxy/wayland_proxy.h"

namespace wl {

namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UP;

class WaylandGlobalEventWaiter : public WestonTestInputEmulate::Observer {
 public:
  enum class WaylandEventType {
    kMotion,
    kButton,
    kKey,
    kTouch,
    kUnknown,
  };

  WaylandGlobalEventWaiter(WaylandEventType event_type,
                           WestonTestInputEmulate* emulate)
      : event_type_(event_type), emulate_(emulate) {
    Initialize();
  }

  WaylandGlobalEventWaiter(WaylandEventType event_type,
                           int button,
                           bool pressed,
                           WestonTestInputEmulate* emulate)
      : event_type_(event_type),
        emulate_(emulate),
        button_or_key_(button),
        pressed_(pressed) {
    Initialize();
  }
  ~WaylandGlobalEventWaiter() override = default;

  // This method assumes that a request has already been queued, with metadata
  // for the expected response stored on the WaylandGlobalEventWaiter. This:
  //   * Flushes queued requests.
  //   * Spins a nested run loop waiting for the expected response (event).
  //   * When the event is received, synchronously flushes all pending
  //     requests and events via wl_display_roundtrip_queue. See
  //     https://crbug.com/1336706#c11 for why this is necessary.
  //   * Dispatches the QuitClosure to avoid re-entrancy.
  void Wait() {
    // There's no guarantee that a flush has been scheduled. Given that we're
    // waiting for a response, we must manually flush.
    wl::WaylandProxy::GetInstance()->FlushForTesting();

    // We disallow nestable tasks as that can result in re-entrancy if the test
    // is listening for side-effects from a wayland-event and then calls
    // PostTask. By using a non-nestable run loop we are relying on the
    // assumption that the ozone-wayland implementation does not rely on
    // PostTask.
    base::RunLoop run_loop;

    // This will be invoked causing the run-loop to quit once the expected event
    // is received.
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void Initialize() {
    DCHECK_NE(event_type_, WaylandEventType::kUnknown);
    emulate_->AddObserver(this);
  }

  void OnPointerMotionGlobal(const gfx::Point& screen_position) override {
    if (event_type_ == WaylandEventType::kMotion) {
      QuitRunLoop();
    }
  }

  void OnPointerButtonGlobal(int32_t button, bool pressed) override {
    if (event_type_ == WaylandEventType::kButton && button_or_key_ == button &&
        pressed == pressed_) {
      QuitRunLoop();
    }
  }

  void OnKeyboardKey(int32_t key, bool pressed) override {
    if (event_type_ == WaylandEventType::kKey && button_or_key_ == key &&
        pressed == pressed_) {
      QuitRunLoop();
    }
  }

  void OnTouchReceived(const gfx::Point& screen_position) override {
    if (event_type_ == WaylandEventType::kTouch) {
      QuitRunLoop();
    }
  }

  void QuitRunLoop() {
    // Immediately remove the observer so that no further callbacks are posted.
    emulate_->RemoveObserver(this);

    // The weston-test protocol does not map cleanly onto ui controls semantics.
    // We need to wait for a wayland round-trip to ensure that all side-effects
    // have been processed. See https://crbug.com/1336706#c11 for details.
    wl::WaylandProxy::GetInstance()->RoundTripQueue();

    // We're in a nested run-loop that doesn't support re-entrancy. Directly
    // invoke the quit closure.
    std::move(quit_closure_).Run();
  }

  // Expected event type.
  WaylandEventType event_type_ = WaylandEventType::kUnknown;

  // Internal closure used to quit the nested run loop.
  base::RepeatingClosure quit_closure_;

  const raw_ptr<WestonTestInputEmulate> emulate_;

  // Expected button or key.
  int button_or_key_ = -1;

  // Expected key or button to be pressed or depressed.
  bool pressed_ = false;
};

}  // namespace

WestonTestOzoneUIControlsTestHelper::WestonTestOzoneUIControlsTestHelper()
    : input_emulate_(std::make_unique<WestonTestInputEmulate>()) {}

WestonTestOzoneUIControlsTestHelper::~WestonTestOzoneUIControlsTestHelper() =
    default;

void WestonTestOzoneUIControlsTestHelper::Reset() {
  input_emulate_->Reset();
}

bool WestonTestOzoneUIControlsTestHelper::SupportsScreenCoordinates() const {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return true;
#else
  return false;
#endif
}

unsigned WestonTestOzoneUIControlsTestHelper::ButtonDownMask() const {
  return button_down_mask_;
}

void WestonTestOzoneUIControlsTestHelper::SendKeyEvents(
    gfx::AcceleratedWidget widget,
    ui::KeyboardCode key,
    int key_event_types,
    int accelerator_state,
    base::OnceClosure closure) {
  if (key_event_types & ui_controls::kKeyPress) {
    SendKeyPressInternal(widget, key, accelerator_state,
                         key_event_types & ui_controls::kKeyRelease
                             ? base::OnceClosure()
                             : std::move(closure),
                         true);
  }
  if (key_event_types & ui_controls::kKeyRelease) {
    SendKeyPressInternal(widget, key, accelerator_state, std::move(closure),
                         false);
  }
}

void WestonTestOzoneUIControlsTestHelper::SendMouseMotionNotifyEvent(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_screen_loc,
    base::OnceClosure closure) {
  WaylandGlobalEventWaiter waiter(
      WaylandGlobalEventWaiter::WaylandEventType::kMotion,
      input_emulate_.get());
  input_emulate_->EmulatePointerMotion(widget, mouse_loc, mouse_screen_loc);
  waiter.Wait();

  if (!closure.is_null()) {
    // PostTask to avoid re-entrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
}

void WestonTestOzoneUIControlsTestHelper::SendMouseEvent(
    gfx::AcceleratedWidget widget,
    ui_controls::MouseButton type,
    int button_state,
    int accelerator_state,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_screen_loc,
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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  SendMouseMotionNotifyEvent(widget, mouse_loc, mouse_screen_loc, {});
#endif

  // Press accelerator keys.
  if (accelerator_state) {
    SendKeyPressInternal(widget, ui::KeyboardCode::VKEY_UNKNOWN,
                         accelerator_state, {}, true);
  }

  if (button_state & DOWN) {
    WaylandGlobalEventWaiter waiter(
        WaylandGlobalEventWaiter::WaylandEventType::kButton, changed_button,
        /*pressed=*/true, input_emulate_.get());
    input_emulate_->EmulatePointerButton(
        widget, ui::EventType::ET_MOUSE_PRESSED, changed_button);
    button_down_mask_ |= changed_button;
    waiter.Wait();
  }
  if (button_state & UP) {
    WaylandGlobalEventWaiter waiter(
        WaylandGlobalEventWaiter::WaylandEventType::kButton, changed_button,
        /*pressed=*/false, input_emulate_.get());
    input_emulate_->EmulatePointerButton(
        widget, ui::EventType::ET_MOUSE_RELEASED, changed_button);
    button_down_mask_ = (button_down_mask_ | changed_button) ^ changed_button;
    waiter.Wait();
  }

  // Depress accelerator keys.
  if (accelerator_state) {
    SendKeyPressInternal(widget, ui::KeyboardCode::VKEY_UNKNOWN,
                         accelerator_state, {}, false);
  }
  if (!closure.is_null()) {
    // PostTask to avoid re-entrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WestonTestOzoneUIControlsTestHelper::SendTouchEvent(
    gfx::AcceleratedWidget widget,
    int action,
    int id,
    const gfx::Point& touch_loc,
    base::OnceClosure closure) {
  // TODO(rivr): ui_controls::TouchType is a bitmask, do we need to handle the
  // case where multiple actions are requested together?
  ui::EventType event_type;
  switch (action) {
    case ui_controls::kTouchPress:
      event_type = ui::EventType::ET_TOUCH_PRESSED;
      break;
    case ui_controls::kTouchRelease:
      event_type = ui::EventType::ET_TOUCH_RELEASED;
      break;
    default:
      event_type = ui::EventType::ET_TOUCH_MOVED;
  }

  WaylandGlobalEventWaiter waiter(
      WaylandGlobalEventWaiter::WaylandEventType::kTouch, input_emulate_.get());
  input_emulate_->EmulateTouch(widget, event_type, id, touch_loc);
  waiter.Wait();
  if (!closure.is_null()) {
    // PostTask to avoid re-entrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
}
#endif

void WestonTestOzoneUIControlsTestHelper::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  NOTREACHED();
}

bool WestonTestOzoneUIControlsTestHelper::MustUseUiControlsForMoveCursorTo() {
  return true;
}

void WestonTestOzoneUIControlsTestHelper::SendKeyPressInternal(
    gfx::AcceleratedWidget widget,
    ui::KeyboardCode key,
    int accelerator_state,
    base::OnceClosure closure,
    bool press_key) {
  auto dom_code = UsLayoutKeyboardCodeToDomCode(key);

  if (press_key) {
    if (accelerator_state & ui_controls::kControl) {
      DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED,
                       ui::DomCode::CONTROL_LEFT);
    }

    if (accelerator_state & ui_controls::kShift) {
      DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED,
                       ui::DomCode::SHIFT_LEFT);
    }

    if (accelerator_state & ui_controls::kAlt) {
      DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED,
                       ui::DomCode::ALT_LEFT);
    }

    DispatchKeyPress(widget, ui::EventType::ET_KEY_PRESSED, dom_code);
  } else {
    DispatchKeyPress(widget, ui::EventType::ET_KEY_RELEASED, dom_code);

    if (accelerator_state & ui_controls::kAlt) {
      DispatchKeyPress(/*widget=*/0, ui::EventType::ET_KEY_RELEASED,
                       ui::DomCode::ALT_LEFT);
    }

    if (accelerator_state & ui_controls::kShift) {
      DispatchKeyPress(/*widget=*/0, ui::EventType::ET_KEY_RELEASED,
                       ui::DomCode::SHIFT_LEFT);
    }

    if (accelerator_state & ui_controls::kControl) {
      DispatchKeyPress(/*widget=*/0, ui::EventType::ET_KEY_RELEASED,
                       ui::DomCode::CONTROL_LEFT);
    }
  }
  if (!closure.is_null()) {
    // PostTask to avoid re-entrancy.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }
}

void WestonTestOzoneUIControlsTestHelper::DispatchKeyPress(
    gfx::AcceleratedWidget widget,
    ui::EventType event_type,
    ui::DomCode dom_code) {
  WaylandGlobalEventWaiter waiter(
      WaylandGlobalEventWaiter::WaylandEventType::kKey,
      ui::KeycodeConverter::DomCodeToEvdevCode(dom_code),
      /*press_key=*/event_type == ui::EventType::ET_KEY_PRESSED,
      input_emulate_.get());
  input_emulate_->EmulateKeyboardKey(widget, event_type, dom_code);
  waiter.Wait();
}

}  // namespace wl

namespace ui {

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperWayland() {
  return new wl::WestonTestOzoneUIControlsTestHelper();
}

}  // namespace ui
