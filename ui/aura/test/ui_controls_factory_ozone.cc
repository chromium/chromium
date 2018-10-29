// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"

namespace aura {
namespace test {
namespace {

// Callback from Window Service with the result of posting an event. |result|
// is true if event successfully processed and |closure| is an optional closure
// to run when done (used in client code to wait for ack).
void OnWindowServiceProcessedEvent(base::OnceClosure closure, bool result) {
  DCHECK(result);
  if (closure)
    std::move(closure).Run();
}

class UIControlsOzone : public ui_controls::UIControlsAura {
 public:
  UIControlsOzone(WindowTreeHost* host) : host_(host) {}

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

    if (control) {
      flags |= ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, flags,
                   base::OnceClosure());
    }

    if (shift) {
      flags |= ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, flags,
                   base::OnceClosure());
    }

    if (alt) {
      flags |= ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_MENU, flags,
                   base::OnceClosure());
    }

    if (command) {
      flags |= ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LWIN, flags,
                   base::OnceClosure());
    }

    PostKeyEvent(ui::ET_KEY_PRESSED, key, flags, base::OnceClosure());
    const bool has_modifier = control || shift || alt || command;
    // Pass the real closure to the last generated KeyEvent.
    PostKeyEvent(ui::ET_KEY_RELEASED, key, flags,
                 has_modifier ? base::OnceClosure() : std::move(closure));

    if (alt) {
      flags &= ~ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_MENU, flags,
                   (shift || control || command) ? base::OnceClosure()
                                                 : std::move(closure));
    }

    if (shift) {
      flags &= ~ui::EF_SHIFT_DOWN;
      PostKeyEvent(
          ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, flags,
          (control || command) ? base::OnceClosure() : std::move(closure));
    }

    if (control) {
      flags &= ~ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, flags,
                   command ? base::OnceClosure() : std::move(closure));
    }

    if (command) {
      flags &= ~ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_LWIN, flags,
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
    // The location needs to be in display's coordinate.
    gfx::Point display_location(screen_x, screen_y);
    display::Display display;
    if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(
            host_->GetDisplayId(), &display)) {
      LOG(ERROR) << "Failed to see the display for " << host_->GetDisplayId();
      return false;
    }
    display_location -= display.bounds().OffsetFromOrigin();

    gfx::Point host_location = display_location;
    host_->ConvertDIPToPixels(&host_location);
    last_mouse_location_ = host_location;

    ui::EventType event_type;

    if (button_down_mask_)
      event_type = ui::ET_MOUSE_DRAGGED;
    else
      event_type = ui::ET_MOUSE_MOVED;

    PostMouseEvent(event_type, host_location, button_down_mask_, 0,
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
    gfx::Point host_location;
    if (last_mouse_location_.has_value()) {
      host_location = last_mouse_location_.value();
    } else {
      // The location needs to be in display's coordinate.
      gfx::Point display_location =
          host_->window()->env()->last_mouse_location();
      display::Display display;
      if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(
              host_->GetDisplayId(), &display)) {
        LOG(ERROR) << "Failed to see the display for " << host_->GetDisplayId();
        return false;
      }
      display_location -= display.bounds().OffsetFromOrigin();

      host_location = display_location;
      host_->ConvertDIPToPixels(&host_location);
    }

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
                     button_down_mask_ | flag, changed_button_flag,
                     (button_state & ui_controls::UP) ? base::OnceClosure()
                                                      : std::move(closure));
    }
    if (button_state & ui_controls::UP) {
      button_down_mask_ &= ~flag;
      PostMouseEvent(ui::ET_MOUSE_RELEASED, host_location,
                     button_down_mask_ | flag, changed_button_flag,
                     std::move(closure));
    }

    return true;
  }
  bool SendMouseClick(ui_controls::MouseButton type) override {
    return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN,
                           ui_controls::kNoAccelerator);
  }

 private:
  void SendEventToSink(ui::Event* event, base::OnceClosure closure) {
    if (host_->window()->env()->mode() == aura::Env::Mode::MUS) {
      GetEventInjector()->InjectEvent(
          host_->GetDisplayId(), ui::Event::Clone(*event),
          base::BindOnce(&OnWindowServiceProcessedEvent, std::move(closure)));
      return;
    }

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
                    base::OnceClosure closure) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&UIControlsOzone::PostKeyEventTask,
                                  base::Unretained(this), type, key_code, flags,
                                  std::move(closure)));
  }

  void PostKeyEventTask(ui::EventType type,
                        ui::KeyboardCode key_code,
                        int flags,
                        base::OnceClosure closure) {
    // Do not rewrite injected events. See crbug.com/136465.
    flags |= ui::EF_FINAL;

    ui::KeyEvent key_event(type, key_code, flags);
    SendEventToSink(&key_event, std::move(closure));
  }

  void PostMouseEvent(ui::EventType type,
                      const gfx::Point& host_location,
                      int flags,
                      int changed_button_flags,
                      base::OnceClosure closure) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UIControlsOzone::PostMouseEventTask,
                       base::Unretained(this), type, host_location, flags,
                       changed_button_flags, std::move(closure)));
  }

  void PostMouseEventTask(ui::EventType type,
                          const gfx::Point& host_location,
                          int flags,
                          int changed_button_flags,
                          base::OnceClosure closure) {
    ui::MouseEvent mouse_event(type, host_location, host_location,
                               ui::EventTimeForNow(), flags,
                               changed_button_flags);

    // This hack is necessary to set the repeat count for clicks.
    ui::MouseEvent mouse_event2(&mouse_event);

    SendEventToSink(&mouse_event2, std::move(closure));
  }

  // Returns the ws::mojom::EventInjector, which is used to send events
  // to the Window Service for dispatch.
  ws::mojom::EventInjector* GetEventInjector() {
    DCHECK_EQ(aura::Env::Mode::MUS, host_->window()->env()->mode());
    if (!event_injector_) {
      DCHECK(aura::test::EnvTestHelper().GetWindowTreeClient());
      aura::test::EnvTestHelper()
          .GetWindowTreeClient()
          ->connector()
          ->BindInterface(ws::mojom::kServiceName, &event_injector_);
    }
    return event_injector_.get();
  }

  WindowTreeHost* host_;
  ws::mojom::EventInjectorPtr event_injector_;

  // The mouse location for the last SendMouseEventsNotifyWhenDone call. This is
  // used rather than Env::last_mouse_location() as Env::last_mouse_location()
  // is updated asynchronously with mus.
  base::Optional<gfx::Point> last_mouse_location_;

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
