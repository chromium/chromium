// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include <algorithm>

#include "base/check.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/input_protection/input_protection_specification.h"
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

// Returns true if the `occluding_rect` contains the `target_point` (used for
// located events like clicks).
bool Occludes(const gfx::Rect& occluding_rect, const gfx::Point& target_point) {
  return occluding_rect.Contains(target_point);
}

// Returns true if the `occluding_rect` occludes any of the `target_rects`
// (used for non-located events like keys). If `check_intersection` is true,
// checks for intersection (partial occlusion); otherwise requires the
// `occluding_rect` to fully contain the target rect.
bool Occludes(const gfx::Rect& occluding_rect,
              const std::vector<gfx::Rect>& target_rects,
              bool check_intersection = true) {
  return std::ranges::any_of(target_rects, [&](const auto& rect) {
    return check_intersection ? occluding_rect.Intersects(rect)
                              : occluding_rect.Contains(rect);
  });
}

// Walks up the view tree to gather and accumulate all input protection bounds.
std::vector<gfx::Rect> GetViewProtectedBounds(const View& target_view) {
  std::vector<gfx::Rect> accumulated_bounds;
  for (const View* view = &target_view; view; view = view->parent()) {
    InputProtectionSpecification* spec = view->GetProperty(kInputProtectionKey);
    if (spec) {
      std::vector<gfx::Rect> bounds = spec->GetProtectedBoundsInScreen(*view);
      accumulated_bounds.insert(accumulated_bounds.end(), bounds.begin(),
                                bounds.end());
    }
  }
  return accumulated_bounds;
}

// Returns true if `point` is inside any of the `rects`.
bool ContainsPoint(const std::vector<gfx::Rect>& rects,
                   const gfx::Point& point) {
  return std::ranges::any_of(
      rects, [&point](const auto& rect) { return rect.Contains(point); });
}

// Returns true if the widget associated with `view` (or its primary window
// widget) has input protection enabled.
bool NeedsInputEventActivationProtection(const View& view) {
  const Widget* widget = view.GetWidget();
  if (!widget) {
    return false;
  }
  const Widget* primary = widget->GetPrimaryWindowWidget();
  return widget->IsInputEventActivationProtectionEnabled() ||
         (primary && primary->IsInputEventActivationProtectionEnabled());
}

}  // namespace

// static
OccludedWidgetInputProtector* OccludedWidgetInputProtector::GetInstance() {
  return base::Singleton<OccludedWidgetInputProtector>::get();
}

OccludedWidgetInputProtector::OccludedWidgetInputProtector() = default;

OccludedWidgetInputProtector::~OccludedWidgetInputProtector() = default;

bool OccludedWidgetInputProtector::CheckPointOcclusion(
    const gfx::Point& target) const {
  // Current (live) Occlusion: Block if any visible always-on-top widget
  // currently occludes the target area.
  for (const auto& [widget, widget_bounds] : always_on_top_widgets_) {
    if (Occludes(widget_bounds, target)) {
      return true;
    }
  }

  // Historical Occlusion: Protects non always-on-top widgets from programmatic
  // state changes (e.g. pop-away attacks where an AOT window is hidden).
  for (const auto& record : occlusion_history_) {
    if (!IsRecordExpired(record) && Occludes(record.bounds, target)) {
      return true;
    }
  }

  return false;
}

bool OccludedWidgetInputProtector::CheckRectsOcclusion(
    const std::vector<gfx::Rect>& target,
    bool check_intersection) const {
  // Current (live) Occlusion: Block if any visible always-on-top widget
  // currently occludes the target area.
  for (const auto& [widget, widget_bounds] : always_on_top_widgets_) {
    if (Occludes(widget_bounds, target, check_intersection)) {
      return true;
    }
  }

  // Historical Occlusion: Protects non always-on-top widgets from programmatic
  // state changes (e.g. pop-away attacks where an AOT window is hidden).
  for (const auto& record : occlusion_history_) {
    if (!IsRecordExpired(record) &&
        Occludes(record.bounds, target, check_intersection)) {
      return true;
    }
  }

  return false;
}

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

  // Only protect against non-located events if the target widget (or its
  // primary window) has explicitly opted-in to input event activation
  // protection.
  if (!event.IsLocatedEvent() &&
      !NeedsInputEventActivationProtection(target_view)) {
    return false;
  }

  std::vector<gfx::Rect> screen_bounds = GetViewProtectedBounds(target_view);
  const bool has_protected_bounds = !screen_bounds.empty();
  if (!has_protected_bounds) {
    screen_bounds = {target_view.GetBoundsInScreen()};
  }

  if (event.IsLocatedEvent()) {
    gfx::Point screen_location = event.AsLocatedEvent()->location();
    View::ConvertPointToScreen(&target_view, &screen_location);

    // Verify the event target. We only block the located event if it landed
    // inside a protected area (view-defined protected bounds or default view
    // bounds) and that area is currently or was recently occluded by an
    // always-on-top window. Located events targeting non-protected areas are
    // never blocked.
    return ContainsPoint(screen_bounds, screen_location) &&
           CheckPointOcclusion(screen_location);
  }

  // Since non-located events do not target a specific point, we must check the
  // occlusion of the target area. If the view defines protected bounds, we
  // block the event if any part of them is occluded. Otherwise (for views
  // without explicitly defined protected bounds), we only block if the view is
  // fully occluded.
  return CheckRectsOcclusion(screen_bounds,
                             /*check_intersection=*/has_protected_bounds);
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
  if (!widget_observations_.IsObservingSource(widget)) {
    widget_observations_.AddObservation(widget);
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
  if (widget_observations_.IsObservingSource(widget)) {
    widget_observations_.RemoveObservation(widget);
  }
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
