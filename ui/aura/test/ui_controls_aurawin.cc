// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/test/ui_controls_internal_win.h"

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

  UIControlsWin(const UIControlsWin&) = delete;
  UIControlsWin& operator=(const UIControlsWin&) = delete;

  // UIControlsAura overrides:
  bool SendKeyEvents(gfx::NativeWindow native_window,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state) override {
    // UIControlsWin only supports key events with both press and release.
    // TODO(crbug.com/40256300): Support any `key_event_types` on win.
    DCHECK_EQ(key_event_types,
              ui_controls::kKeyPress | ui_controls::kKeyRelease);

    // No command key on Aura.
    DCHECK(!(accelerator_state & ui_controls::kCommand));

    HWND window = native_window->GetHost()->GetAcceleratedWidget();
    return ui_controls::internal::SendKeyPressReleaseImpl(
        window, key, accelerator_state, ui_controls::KeyEventType::kKeyRelease,
        base::OnceClosure());
  }
  bool SendKeyEventsNotifyWhenDone(
      gfx::NativeWindow native_window,
      ui::KeyboardCode key,
      int key_event_types,
      base::OnceClosure task,
      int accelerator_state,
      ui_controls::KeyEventType wait_for) override {
    // UIControlsWin only supports key events with both press and release.
    // TODO(crbug.com/40256300): Support any `key_event_types` on win.
    DCHECK_EQ(key_event_types,
              ui_controls::kKeyPress | ui_controls::kKeyRelease);

    // No command key on Aura.
    DCHECK(!(accelerator_state & ui_controls::kCommand));

    HWND window = native_window->GetHost()->GetAcceleratedWidget();
    return ui_controls::internal::SendKeyPressReleaseImpl(
        window, key, accelerator_state, wait_for, std::move(task));
  }

  bool SendMouseMove(int screen_x, int screen_y) override {
    return ui_controls::internal::SendMouseMoveImpl(screen_x, screen_y,
                                                    base::OnceClosure());
  }
  bool SendMouseMoveNotifyWhenDone(int screen_x,
                                   int screen_y,
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
};

}  // namespace

namespace aura::test {

void EnableUIControlsAuraWin() {
  InstallUIControlsAura(new UIControlsWin());
}

}  // namespace aura::test
