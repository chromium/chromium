// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view_targeter.h"

#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/views_switches.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

RootViewTargeter::RootViewTargeter(ViewTargeterDelegate* delegate,
                                   internal::RootView* root_view)
    : ViewTargeter(delegate), root_view_(root_view) {}

RootViewTargeter::~RootViewTargeter() = default;

View* RootViewTargeter::FindTargetForGestureEvent(
    View* root,
    const ui::GestureEvent& gesture) {
  CHECK_EQ(root, root_view_);

  // Return the default gesture handler if one is already set.
  if (root_view_->gesture_handler_) {
    CHECK(root_view_->gesture_handler_set_before_processing_);
    return root_view_->gesture_handler_;
  }

  // If non-empty, use the gesture's bounding box to determine the target.
  // Otherwise use the center point of the gesture's bounding box.
  gfx::Rect rect(gesture.location(), gfx::Size(1, 1));
  if (!gesture.details().bounding_box().IsEmpty()) {
    // TODO(tdanderson): Pass in the bounding box to GetEventHandlerForRect()
    // once crbug.com/313392 is resolved.
    rect.set_size(gesture.details().bounding_box().size());
    rect.Offset(-rect.width() / 2, -rect.height() / 2);
  }

  return root->GetEffectiveViewTargeter()->TargetForRect(root, rect);
}

ui::EventTarget* RootViewTargeter::FindNextBestTargetForGestureEvent(
    ui::EventTarget* previous_target,
    const ui::GestureEvent& gesture) {
  // ET_GESTURE_END events should only ever be targeted to the default
  // gesture handler set by a previous gesture, if one exists. Thus we do not
  // permit any re-targeting of ET_GESTURE_END events.
  if (gesture.type() == ui::ET_GESTURE_END)
    return nullptr;

  // Prohibit re-targeting of gesture events (except for GESTURE_SCROLL_BEGIN
  // events) if the default gesture handler was set by the dispatch of a
  // previous gesture event.
  if (root_view_->gesture_handler_set_before_processing_ &&
      gesture.type() != ui::ET_GESTURE_SCROLL_BEGIN) {
    return nullptr;
  }

  // If |gesture_handler_| is NULL, it is either because the view was removed
  // from the tree by the previous dispatch of |gesture| or because |gesture| is
  // the GESTURE_END event corresponding to the removal of the last touch
  // point. In either case, no further re-targeting of |gesture| should be
  // permitted.
  if (!root_view_->gesture_handler_)
    return nullptr;

  return previous_target->GetParentTarget();
}

}  // namespace views
