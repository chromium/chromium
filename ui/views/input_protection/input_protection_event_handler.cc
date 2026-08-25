// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_event_handler.h"

#include <cstdint>
#include <vector>

#include "base/check.h"
#include "ui/events/event.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool HasInputProtectedProperty(const ui::Event& event) {
  const auto* properties = event.properties();
  return properties && properties->contains(kPropertyInputProtected);
}

void SetInputProtectedProperty(ui::Event* event) {
  event->SetProperty(kPropertyInputProtected, std::vector<uint8_t>());
}

}  // namespace

InputProtectionEventHandler::InputProtectionEventHandler(
    internal::RootView* root_view)
    : root_view_(root_view) {
  CHECK(root_view_);
  root_view_->AddPreTargetHandler(this);
}

InputProtectionEventHandler::~InputProtectionEventHandler() {
  root_view_->RemovePreTargetHandler(this);
}

void InputProtectionEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::EventType::kTouchPressed) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::EventType::kGestureTap &&
      type != ui::EventType::kGestureTapDown &&
      type != ui::EventType::kGestureDoubleTap &&
      type != ui::EventType::kGestureLongPress &&
      type != ui::EventType::kGestureLongTap) {
    return;
  }

  MaybeBlockEvent(event);
}

void InputProtectionEventHandler::MaybeBlockEvent(ui::Event* event) {
  if (!event->cancelable()) {
    return;
  }

  auto* target_view = static_cast<View*>(event->target());
  Widget* widget = target_view ? target_view->GetWidget() : nullptr;
  if (!widget) {
    return;
  }

  // If this event has already been through the input protection framework,
  // skip it to prevent duplicate evaluations.
  if (HasInputProtectedProperty(*event)) {
    return;
  }

  SetInputProtectedProperty(event);

  auto* input_protector = widget->GetInputEventActivationProtector();
  if (!input_protector ||
      !input_protector->IsPossiblyUnintendedInteraction(
          *event, /*allow_key_events=*/false, target_view)) {
    return;
  }

  // Discard the unintended event to prevent it from reaching the target view.
  event->StopPropagation();

  // Reset the RootView handler state so that any subsequent events from this
  // blocked interaction (e.g. mouse release) are discarded.
  root_view_->ResetEventHandlers();
}

}  // namespace views
