// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_EVENT_HANDLER_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_EVENT_HANDLER_H_

#include <optional>
#include <string_view>

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
  // Reasons why an input event was blocked by the Views input protection
  // framework.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(InputProtectionBlockReason)
  enum class InputProtectionBlockReason {
    kAlwaysOnTopOcclusion = 0,
    kInputProtectionPolicy = 1,
    kMaxValue = kInputProtectionPolicy,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/security/enums.xml:InputProtectionBlockReason)

  // Identifies a protected widget for input protection metrics.
  //
  // LINT.IfChange(InputProtectionWidgetIdentifier)
  enum class InputProtectionWidgetIdentifier {
    kTestWidget = 0,
    kMaxValue = kTestWidget,
  };
  // LINT.ThenChange(//ui/views/input_protection/input_protection_event_handler.cc:InputProtectionWidgetIdentifier)

  // Returns the string representation of `widget_identifier`.
  static std::string_view GetWidgetIdentifierString(
      InputProtectionWidgetIdentifier widget_identifier);

  explicit InputProtectionEventHandler(internal::RootView* root_view);
  ~InputProtectionEventHandler() override;

  InputProtectionEventHandler(const InputProtectionEventHandler&) = delete;
  InputProtectionEventHandler& operator=(const InputProtectionEventHandler&) =
      delete;

  void set_widget_identifier(
      std::optional<InputProtectionWidgetIdentifier> widget_identifier) {
    widget_identifier_ = widget_identifier;
  }

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

  // Optional widget identifier for input protection metrics.
  std::optional<InputProtectionWidgetIdentifier> widget_identifier_;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_EVENT_HANDLER_H_
