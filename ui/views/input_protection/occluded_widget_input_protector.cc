// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
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
    const std::set<Widget*>& tracked_widgets) {
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

bool OccludedWidgetInputProtector::ShouldBlockEvent(const ui::Event& event,
                                                    const View& target_view) {
  if (always_on_top_widgets_.empty()) {
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

  for (Widget* widget : always_on_top_widgets_) {
    if (widget->GetNonDecoratedClientAreaBoundsInScreen().Contains(
            screen_location)) {
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
    always_on_top_widgets_.insert(widget);
    return;
  }

  always_on_top_widgets_.erase(widget);
}

void OccludedWidgetInputProtector::Unregister(Widget* widget) {
  always_on_top_widgets_.erase(widget);
  widget->RemoveObserver(this);
}

}  // namespace views
