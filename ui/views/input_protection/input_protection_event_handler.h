// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_EVENT_HANDLER_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_EVENT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/views/views_export.h"

namespace ui {
class Event;
class GestureEvent;
class KeyEvent;
class MouseEvent;
class TouchEvent;
}  // namespace ui

namespace views {

namespace internal {
class RootView;
}

// Event property key set on events that have already been processed by the
// input protection framework to prevent duplicate evaluations on re-dispatch.
inline constexpr char kPropertyInputProtected[] = "views_input_protected";

// Registers as a pre-target handler on the `widget` `RootView` to intercept
// input events and evaluate whether to block them before dispatching to target
// views.
//
// See ui/views/input_protection/README.md for details.
class VIEWS_EXPORT InputProtectionEventHandler : public ui::EventHandler {
 public:
  explicit InputProtectionEventHandler(internal::RootView* root_view);
  ~InputProtectionEventHandler() override;

  InputProtectionEventHandler(const InputProtectionEventHandler&) = delete;
  InputProtectionEventHandler& operator=(const InputProtectionEventHandler&) =
      delete;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Evaluates whether `event` should be blocked, and if so, consumes the event
  // and resets the target widget `RootView` event handlers.
  void MaybeBlockEvent(ui::Event* event);

  // The `RootView` this handler is registered on as a pre-target handler.
  raw_ptr<internal::RootView> root_view_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_EVENT_HANDLER_H_
