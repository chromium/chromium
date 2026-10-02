// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/view_bounds_and_visibility_input_protection_policy.h"

#include <memory>

#include "base/check.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

ViewBoundsAndVisibilityInputProtectionPolicy::
    ViewBoundsAndVisibilityInputProtectionPolicy(View& view)
    : last_bounds_(view.ConvertRectToWidget(view.GetLocalBounds())),
      scoped_notify_visible_bounds_changed_(
          std::make_unique<View::ScopedNotifyObserversOnVisibleBoundsChanged>(
              view)) {
  CHECK(view.GetWidget())
      << "ViewBoundsAndVisibilityInputProtectionPolicy must be constructed "
         "after the View has been added to a Widget (e.g. in "
         "View::AddedToWidget()).";
  view_observation_.Observe(&view);
  if (IsViewDrawnAndWidgetVisible()) {
    OnProtectionStarted();
  }
}

ViewBoundsAndVisibilityInputProtectionPolicy::
    ~ViewBoundsAndVisibilityInputProtectionPolicy() = default;

void ViewBoundsAndVisibilityInputProtectionPolicy::OnProtectionStarted() {
  view_protected_time_stamp_ = base::TimeTicks::Now();
}

void ViewBoundsAndVisibilityInputProtectionPolicy::OnProtectionReset() {
  if (!view_protected_time_stamp_.is_null()) {
    view_protected_time_stamp_ = base::TimeTicks::Now();
  }
}

void ViewBoundsAndVisibilityInputProtectionPolicy::OnProtectionStopped() {
  view_protected_time_stamp_ = base::TimeTicks();
}

bool ViewBoundsAndVisibilityInputProtectionPolicy::
    IsPossiblyUnintendedInteraction(
        const ui::Event& event,
        const View* target_view,
        const InputEventActivationProtector& protector) {
  if (view_protected_time_stamp_.is_null()) {
    return false;
  }

  if (event.time_stamp() >=
      view_protected_time_stamp_ + protector.cooldown_interval()) {
    return false;
  }

  const View* observed_view = view_observation_.GetSource();
  if (!observed_view || !target_view) {
    return false;
  }

  // Only protect interactions targeted at the observed `View` or descendants of
  // the observed `View` (e.g. clicking an icon or label inside a protected
  // button).
  return observed_view->Contains(target_view);
}

void ViewBoundsAndVisibilityInputProtectionPolicy::OnViewVisibleBoundsChanged(
    View* observed_view) {
  const gfx::Rect new_bounds =
      observed_view->ConvertRectToWidget(observed_view->GetLocalBounds());
  if (last_bounds_ == new_bounds) {
    return;
  }

  const bool was_empty = last_bounds_.IsEmpty();
  last_bounds_ = new_bounds;

  // Ignore changes from empty bounds (such as initial layout).
  if (was_empty) {
    return;
  }

  if (!new_bounds.IsEmpty() && IsViewDrawnAndWidgetVisible()) {
    OnProtectionStarted();
  }
}

void ViewBoundsAndVisibilityInputProtectionPolicy::OnViewRemovedFromWidget(
    View* observed_view) {
  scoped_notify_visible_bounds_changed_.reset();
  view_observation_.Reset();
  OnProtectionStopped();
}

void ViewBoundsAndVisibilityInputProtectionPolicy::OnViewVisibilityChanged(
    View* observed_view,
    View* starting_view,
    bool visible) {
  if (visible && IsViewDrawnAndWidgetVisible()) {
    OnProtectionStarted();
  } else {
    OnProtectionStopped();
  }
}

void ViewBoundsAndVisibilityInputProtectionPolicy::OnViewIsDeleting(
    View* observed_view) {
  scoped_notify_visible_bounds_changed_.reset();
  view_observation_.Reset();
  OnProtectionStopped();
}

bool ViewBoundsAndVisibilityInputProtectionPolicy::IsViewDrawnAndWidgetVisible()
    const {
  const View* view = view_observation_.GetSource();
  return view && view->IsDrawn() && view->GetWidget() &&
         view->GetWidget()->IsVisible();
}

}  // namespace views
