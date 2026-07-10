// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include "base/check.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Returns true if the widget's Z-order level is at least floating.
bool IsAlwaysOnTop(Widget* widget) {
  return widget->GetZOrderLevel() >= ui::ZOrderLevel::kFloatingWindow;
}

// Returns true if the `target_view` is associated with a primary window that
// is a tracked always-on-top widget.
bool IsViewAssociatedWithTrackedWidget(
    const View& target_view,
    const std::map<Widget*, gfx::Rect>& tracked_widgets) {
  const Widget* widget = target_view.GetWidget();
  CHECK(widget) << "A view without a widget should never receive an event.";
  const Widget* primary = widget->GetPrimaryWindowWidget();
  return tracked_widgets.contains(const_cast<Widget*>(primary));
}

}  // namespace

// static
OccludedWidgetInputProtector* OccludedWidgetInputProtector::GetInstance() {
  return base::Singleton<OccludedWidgetInputProtector>::get();
}

OccludedWidgetInputProtector::OccludedWidgetInputProtector() = default;

OccludedWidgetInputProtector::~OccludedWidgetInputProtector() = default;

bool OccludedWidgetInputProtector::ShouldBlockEvent(
    const ui::Event& event,
    const View& target_view) const {
  PruneCachedOcclusionHistory();

  if (always_on_top_widgets_.empty() && occlusion_history_.empty()) {
    return false;
  }

  if (IsViewAssociatedWithTrackedWidget(target_view, always_on_top_widgets_)) {
    return false;
  }

  if (!event.IsLocatedEvent()) {
    // TODO(crbug.com/467460499): Determine how to handle non-located events
    // (e.g. keyboard events) once an acceptable accessibility solution is
    // identified.
    return false;
  }

  gfx::Point screen_location = event.AsLocatedEvent()->location();
  View::ConvertPointToScreen(&target_view, &screen_location);

  // Current (live) Occlusion: Block if any visible always-on-top widget
  // currently occludes the location.
  for (const auto& [widget, bounds] : always_on_top_widgets_) {
    if (bounds.Contains(screen_location)) {
      return true;
    }
  }

  // Historical Occlusion: Protects non always-on-top widgets from programmatic
  // state changes (e.g. pop-away attacks where an AOT window is hidden).
  for (const auto& record : occlusion_history_) {
    if (!IsRecordExpired(record) && record.bounds.Contains(screen_location)) {
      return true;
    }
  }

  return false;
}

void OccludedWidgetInputProtector::UpdateTracking(base::PassKey<views::Widget>,
                                                  Widget* widget) {
  UpdateTrackingImpl(widget);
}

void OccludedWidgetInputProtector::OnWidgetVisibilityChanged(Widget* widget,
                                                             bool visible) {
  UpdateTrackingImpl(widget);
}

void OccludedWidgetInputProtector::OnWidgetDestroying(Widget* widget) {
  Unregister(widget);
}

void OccludedWidgetInputProtector::OnWidgetBoundsChanged(
    Widget* widget,
    const gfx::Rect& new_bounds) {
  auto it = always_on_top_widgets_.find(widget);
  if (it == always_on_top_widgets_.end()) {
    return;
  }

  const gfx::Rect current_bounds =
      widget->GetNonDecoratedClientAreaBoundsInScreen();
  if (current_bounds == it->second) {
    // Redundant event; no area has been vacated.
    return;
  }

  // Only record "move" historical occlusion if the widget is currently
  // visible. If it's already hidden, StopTracking will handle the final
  // historical record.
  if (widget->IsVisible() && !IsManuallyManipulated(widget)) {
    // A programmatic move has occurred. Record the old area as occluded.
    RecordHistoricalOcclusion(it->second);
  }

  it->second = current_bounds;
}

void OccludedWidgetInputProtector::OnWidgetUserResizeStarted(Widget* widget) {
  resizing_widgets_.insert(widget);
}

void OccludedWidgetInputProtector::OnWidgetUserResizeEnded(Widget* widget) {
  resizing_widgets_.erase(widget);
}

void OccludedWidgetInputProtector::UpdateTrackingImpl(Widget* widget) {
  if (!base::FeatureList::IsEnabled(features::kEnableInputProtection)) {
    return;
  }

  if (!IsAlwaysOnTop(widget)) {
    Unregister(widget);
    return;
  }

  Register(widget);
}

void OccludedWidgetInputProtector::Register(Widget* widget) {
  if (!widget->HasObserver(this)) {
    widget->AddObserver(this);
  }

  if (widget->IsVisible()) {
    always_on_top_widgets_[widget] =
        widget->GetNonDecoratedClientAreaBoundsInScreen();
    return;
  }

  StopTracking(widget);
}

void OccludedWidgetInputProtector::Unregister(Widget* widget) {
  StopTracking(widget);
  widget->RemoveObserver(this);
}

void OccludedWidgetInputProtector::StopTracking(Widget* widget) {
  if (const auto it = always_on_top_widgets_.find(widget);
      it != always_on_top_widgets_.end()) {
    if (!IsManuallyManipulated(widget)) {
      RecordHistoricalOcclusion(it->second);
    }
    always_on_top_widgets_.erase(it);
  }
  resizing_widgets_.erase(widget);
}

bool OccludedWidgetInputProtector::IsManuallyManipulated(Widget* widget) const {
  return widget->is_dragging() || resizing_widgets_.contains(widget);
}

void OccludedWidgetInputProtector::RecordHistoricalOcclusion(
    const gfx::Rect& bounds) {
  PruneCachedOcclusionHistory();

  if (bounds.IsEmpty()) {
    return;
  }

  occlusion_history_.push_back(
      {.bounds = bounds, .timestamp = base::TimeTicks::Now()});
}

void OccludedWidgetInputProtector::PruneCachedOcclusionHistory() const {
  while (!occlusion_history_.empty() &&
         IsRecordExpired(occlusion_history_.front())) {
    occlusion_history_.pop_front();
  }
}

bool OccludedWidgetInputProtector::IsRecordExpired(
    const HistoricalOcclusion& record) const {
  // We use the double-click interval because it is the standard
  // OS-level timing for distinguishing separate user interactions. This
  // prevents accidental fall-through clicks from rapid clicking or sudden UI
  // reveals without impacting perceived responsiveness.
  return record.timestamp <= base::TimeTicks::Now() - GetDoubleClickInterval();
}

void OccludedWidgetInputProtector::ClearForTesting() {
  always_on_top_widgets_.clear();
  occlusion_history_.clear();
  resizing_widgets_.clear();
}

}  // namespace views
