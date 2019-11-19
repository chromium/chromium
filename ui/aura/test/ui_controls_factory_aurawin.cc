// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/test/ui_controls_internal_win.h"

namespace aura {
namespace test {

namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UIControlsAura;
using ui_controls::UP;

class UIControlsWin : public UIControlsAura {
 public:
  UIControlsWin() {}

  // UIControlsAura overrides:
  bool SendKeyPress(gfx::NativeWindow native_window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    DCHECK(!command);  // No command key on Aura
    HWND window =
        native_window->GetHost()->GetAcceleratedWidget();
    return ui_controls::internal::SendKeyPressImpl(window, key, control, shift,
                                                   alt, base::OnceClosure());
  }
  bool SendKeyPressNotifyWhenDone(gfx::NativeWindow native_window,
                                  ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt,
                                  bool command,
                                  base::OnceClosure task) override {
    DCHECK(!command);  // No command key on Aura
    HWND window =
        native_window->GetHost()->GetAcceleratedWidget();
    return ui_controls::internal::SendKeyPressImpl(window, key, control, shift,
                                                   alt, std::move(task));
  }
  bool SendMouseMove(long screen_x, long screen_y) override {
    return ui_controls::internal::SendMouseMoveImpl(screen_x, screen_y,
                                                    base::OnceClosure());
  }
  bool SendMouseMoveNotifyWhenDone(long screen_x,
                                   long screen_y,
                                   base::OnceClosure task) override {
    return ui_controls::internal::SendMouseMoveImpl(screen_x, screen_y,
                                                    std::move(task));
  }
  bool SendMouseEvents(MouseButton type,
                       int button_state,
                       int accelerator_state) override {
    return ui_controls::internal::SendMouseEventsImpl(
        type, button_state, base::OnceClosure(), accelerator_state);
  }
  bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                     int button_state,
                                     base::OnceClosure task,
                                     int accelerator_state) override {
    return ui_controls::internal::SendMouseEventsImpl(
        type, button_state, std::move(task), accelerator_state);
  }
  bool SendMouseClick(MouseButton type) override {
    return SendMouseEvents(type, UP | DOWN, ui_controls::kNoAccelerator);
  }
  bool SendTouchEvents(int action, int num, int x, int y) override {
    return ui_controls::internal::SendTouchEventsImpl(action, num, x, y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UIControlsWin);
};

}  // namespace

UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsWin();
}

}  // namespace test
}  // namespace aura
