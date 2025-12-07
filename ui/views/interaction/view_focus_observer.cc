// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/view_focus_observer.h"

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views::test {

ViewFocusObserverBase::ViewFocusObserverBase(Widget* widget)
    : focus_manager_(widget->GetFocusManager()) {
  CHECK_EQ(focus_manager_, widget->GetTopLevelWidget()->GetFocusManager())
      << "Expect focus manager to be owned by top level widget.";
  widget_observation_.Observe(widget->GetTopLevelWidget());
  focus_manager_->AddFocusChangeListener(this);
}

ViewFocusObserverBase::~ViewFocusObserverBase() {
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
  }
}

View* ViewFocusObserverBase::GetFocusedView() const {
  return focus_manager_ ? focus_manager_->GetFocusedView() : nullptr;
}

void ViewFocusObserverBase::OnWillChangeFocus(View* from, View* to) {}

void ViewFocusObserverBase::OnDidChangeFocus(View* from, View* to) {
  OnFocusChanged(to);
}

void ViewFocusObserverBase::OnWidgetDestroying(Widget* widget) {
  if (focus_manager_) {
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
  }
  widget_observation_.Reset();
  OnFocusChanged(nullptr);
}

ViewFocusObserverByView::ViewFocusObserverByView(Widget* widget)
    : ViewFocusObserverBase(widget) {}

View* ViewFocusObserverByView::GetStateObserverInitialState() const {
  return GetFocusedView();
}

void ViewFocusObserverByView::OnFocusChanged(View* new_focused_view) {
  OnStateObserverStateChanged(new_focused_view);
}

ViewFocusObserverByIdentifier::ViewFocusObserverByIdentifier(Widget* widget)
    : ViewFocusObserverBase(widget) {}

ui::ElementIdentifier
ViewFocusObserverByIdentifier::GetStateObserverInitialState() const {
  auto* const focused = GetFocusedView();
  return focused ? focused->GetProperty(kElementIdentifierKey)
                 : ui::ElementIdentifier();
}

void ViewFocusObserverByIdentifier::OnFocusChanged(View* new_focused_view) {
  OnStateObserverStateChanged(
      new_focused_view ? new_focused_view->GetProperty(kElementIdentifierKey)
                       : ui::ElementIdentifier());
}

DEFINE_STATE_IDENTIFIER_VALUE(ViewFocusObserverByView, kCurrentFocusedView);
DEFINE_STATE_IDENTIFIER_VALUE(ViewFocusObserverByIdentifier,
                              kCurrentFocusedViewId);

}  // namespace views::test
