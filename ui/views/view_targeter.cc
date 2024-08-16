// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ui/views/view_targeter.h"

#include "ui/events/event_target.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {

ViewTargeter::ViewTargeter(ViewTargeterDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

ViewTargeter::ViewTargeter(std::unique_ptr<ViewTargeterDelegate> delegate)
    : owned_delegate_(std::move(delegate)), delegate_(owned_delegate_.get()) {
  DCHECK(delegate_);
}

ViewTargeter::~ViewTargeter() = default;

bool ViewTargeter::DoesIntersectRect(const View* target,
                                     const gfx::Rect& rect) const {
  return delegate_->DoesIntersectRect(target, rect);
}

View* ViewTargeter::TargetForRect(View* root, const gfx::Rect& rect) const {
  return delegate_->TargetForRect(root, rect);
}

ui::EventTarget* ViewTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                  ui::Event* event) {
  View* const view = static_cast<View*>(root);

  if (event->IsKeyEvent())
    return FindTargetForKeyEvent(view, *event->AsKeyEvent());

  if (event->IsScrollEvent())
    return FindTargetForScrollEvent(view, *event->AsScrollEvent());

  CHECK(event->IsGestureEvent())
      << "ViewTargeter does not yet support this event type.";

  ui::GestureEvent* gesture = event->AsGestureEvent();
  View* gesture_target = FindTargetForGestureEvent(view, *gesture);
  root->ConvertEventToTarget(gesture_target, gesture);
  return gesture_target;
}

ui::EventTarget* ViewTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  if (!previous_target)
    return nullptr;

  if (event->IsGestureEvent()) {
    ui::GestureEvent* gesture = event->AsGestureEvent();
    ui::EventTarget* next_target =
        FindNextBestTargetForGestureEvent(previous_target, *gesture);
    previous_target->ConvertEventToTarget(next_target, gesture);
    return next_target;
  }

  return previous_target->GetParentTarget();
}

View* ViewTargeter::FindTargetForKeyEvent(View* root, const ui::KeyEvent& key) {
  if (root->GetFocusManager())
    return root->GetFocusManager()->GetFocusedView();
  return nullptr;
}

View* ViewTargeter::FindTargetForScrollEvent(View* root,
                                             const ui::ScrollEvent& scroll) {
  gfx::Rect rect(scroll.location(), gfx::Size(1, 1));
  return root->GetEffectiveViewTargeter()->TargetForRect(root, rect);
}

View* ViewTargeter::FindTargetForGestureEvent(View* root,
                                              const ui::GestureEvent& gesture) {
  // TODO(tdanderson): The only code path that performs targeting for gestures
  //                   uses the ViewTargeter installed on the RootView (i.e.,
  //                   a RootViewTargeter). Provide a default implementation
  //                   here if we need to be able to perform gesture targeting
  //                   starting at an arbitrary node in a Views tree.
  NOTREACHED();
}

ui::EventTarget* ViewTargeter::FindNextBestTargetForGestureEvent(
    ui::EventTarget* previous_target,
    const ui::GestureEvent& gesture) {
  NOTREACHED();
}

}  // namespace views
