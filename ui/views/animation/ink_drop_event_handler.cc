// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_event_handler.h"

#include <memory>
#include <string_view>

#include "build/build_config.h"
#include "ui/events/scoped_target_handler.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool InkDropStateIsVisible(InkDropState state) {
  return state != InkDropState::HIDDEN && state != InkDropState::DEACTIVATED;
}

}  // namespace

InkDropEventHandler::InkDropEventHandler(View* host_view, Delegate* delegate)
    : target_handler_(
          std::make_unique<ui::ScopedTargetHandler>(host_view, this)),
      host_view_(host_view),
      delegate_(delegate) {
  observation_.Observe(host_view_.get());
}

InkDropEventHandler::~InkDropEventHandler() = default;

void InkDropEventHandler::AnimateToState(InkDropState state,
                                         const ui::LocatedEvent* event) {
#if BUILDFLAG(IS_WIN)
  // On Windows, don't initiate ink-drops for touch/gesture events.
  // Additionally, certain event states should dismiss existing ink-drop
  // animations. If the state is already other than HIDDEN, presumably from
  // a mouse or keyboard event, then the state should be allowed. Conversely,
  // if the requested state is ACTIVATED, then it should always be allowed.
  if (event && (event->IsTouchEvent() || event->IsGestureEvent()) &&
      delegate_->GetInkDrop()->GetTargetInkDropState() ==
          InkDropState::HIDDEN &&
      state != InkDropState::ACTIVATED) {
    return;
  }
#endif
  last_ripple_triggering_event_.reset(
      event ? event->Clone().release()->AsLocatedEvent() : nullptr);

  // If no ink drop exists and we are not transitioning to a visible ink drop
  // state the transition have no visual effect. The call to GetInkDrop() will
  // lazily create the ink drop when called. Avoid creating the ink drop in
  // these cases to prevent the creation of unnecessary layers.
  if (delegate_->HasInkDrop() || InkDropStateIsVisible(state))
    delegate_->GetInkDrop()->AnimateToState(state);
}

ui::LocatedEvent* InkDropEventHandler::GetLastRippleTriggeringEvent() const {
  return last_ripple_triggering_event_.get();
}

void InkDropEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (!host_view_->GetEnabled() || !delegate_->SupportsGestureEvents())
    return;

  InkDropState current_ink_drop_state =
      delegate_->GetInkDrop()->GetTargetInkDropState();

  InkDropState ink_drop_state = InkDropState::HIDDEN;
  switch (event->type()) {
    case ui::EventType::kGestureTapDown:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
      ink_drop_state = InkDropState::ACTION_PENDING;
      break;
    case ui::EventType::kGestureLongPress:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
      ink_drop_state = InkDropState::ALTERNATE_ACTION_PENDING;
      break;
    case ui::EventType::kGestureLongTap:
      ink_drop_state = InkDropState::ALTERNATE_ACTION_TRIGGERED;
      break;
    case ui::EventType::kGestureEnd:
    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kGestureTapCancel:
      if (current_ink_drop_state == InkDropState::ACTIVATED)
        return;
      ink_drop_state = InkDropState::HIDDEN;
      break;
    default:
      return;
  }

  if (ink_drop_state == InkDropState::HIDDEN &&
      (current_ink_drop_state == InkDropState::ACTION_TRIGGERED ||
       current_ink_drop_state == InkDropState::ALTERNATE_ACTION_TRIGGERED ||
       current_ink_drop_state == InkDropState::DEACTIVATED ||
       current_ink_drop_state == InkDropState::HIDDEN)) {
    // These InkDropStates automatically transition to the HIDDEN state so we
    // don't make an explicit call. Explicitly animating to HIDDEN in this
    // case would prematurely pre-empt these animations.
    return;
  }
  AnimateToState(ink_drop_state, event);
}

void InkDropEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::EventType::kMouseEntered:
      delegate_->GetInkDrop()->SetHovered(true);
      break;
    case ui::EventType::kMouseExited:
      delegate_->GetInkDrop()->SetHovered(false);
      break;
    case ui::EventType::kMouseDragged:
      delegate_->GetInkDrop()->SetHovered(
          host_view_->GetLocalBounds().Contains(event->location()));
      break;
    default:
      break;
  }
}

std::string_view InkDropEventHandler::GetLogContext() const {
  return "InkDropEventHandler";
}

void InkDropEventHandler::OnViewVisibilityChanged(View* observed_view,
                                                  View* starting_view) {
  DCHECK_EQ(host_view_, observed_view);
  // A View is *actually* visible if its visible flag is set, all its ancestors'
  // visible flags are set, it's in a Widget, and the Widget is
  // visible. |View::IsDrawn()| captures the first two conditions.
  const bool is_visible = host_view_->IsDrawn() && host_view_->GetWidget() &&
                          host_view_->GetWidget()->IsVisible();
  if (!is_visible && delegate_->HasInkDrop()) {
    delegate_->GetInkDrop()->AnimateToState(InkDropState::HIDDEN);
    delegate_->GetInkDrop()->SetHovered(false);
  }
}

void InkDropEventHandler::OnViewHierarchyChanged(
    View* observed_view,
    const ViewHierarchyChangedDetails& details) {
  DCHECK_EQ(host_view_, observed_view);
  // If we're being removed hide the ink-drop so if we're highlighted now the
  // highlight won't be active if we're added back again.
  if (!details.is_add && details.child == host_view_ &&
      delegate_->HasInkDrop()) {
    delegate_->GetInkDrop()->SnapToHidden();
    delegate_->GetInkDrop()->SetHovered(false);
  }
}

void InkDropEventHandler::OnViewBoundsChanged(View* observed_view) {
  DCHECK_EQ(host_view_, observed_view);
  if (delegate_->HasInkDrop())
    delegate_->GetInkDrop()->HostSizeChanged(host_view_->size());
}

void InkDropEventHandler::OnViewFocused(View* observed_view) {
  DCHECK_EQ(host_view_, observed_view);
  delegate_->GetInkDrop()->SetFocused(true);
}

void InkDropEventHandler::OnViewBlurred(View* observed_view) {
  DCHECK_EQ(host_view_, observed_view);
  delegate_->GetInkDrop()->SetFocused(false);
}

void InkDropEventHandler::OnViewThemeChanged(View* observed_view) {
  CHECK_EQ(host_view_, observed_view);
  // The call to GetInkDrop() will lazily create the ink drop when called. We do
  // not want to create the ink drop when view theme changed.
  if (delegate_->HasInkDrop()) {
    delegate_->GetInkDrop()->HostViewThemeChanged();
  }
}

}  // namespace views
