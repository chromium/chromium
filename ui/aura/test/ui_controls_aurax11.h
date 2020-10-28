// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_UI_CONTROLS_AURAX11_H_
#define UI_AURA_TEST_UI_CONTROLS_AURAX11_H_

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/test/x11_event_sender.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/test/x11_event_waiter.h"

namespace aura {
namespace test {

class UIControlsX11 : public ui_controls::UIControlsAura {
 public:
  explicit UIControlsX11(WindowTreeHost* host);
  UIControlsX11(const UIControlsX11&) = delete;
  UIControlsX11& operator=(const UIControlsX11&) = delete;
  ~UIControlsX11() override;

  // UIControlsAura overrides:
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

  void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure);

 private:
  void SetKeycodeAndSendThenMask(x11::KeyEvent* xevent,
                                 uint32_t keysym,
                                 x11::KeyButMask mask);

  void UnmaskAndSetKeycodeThenSend(x11::KeyEvent* xevent,
                                   x11::KeyButMask mask,
                                   uint32_t keysym);
  WindowTreeHost* const host_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_UI_CONTROLS_AURAX11_H_
