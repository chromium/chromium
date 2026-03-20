// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include "ui/base/ui_base_types.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool IsAlwaysOnTop(Widget* widget) {
  return widget->GetZOrderLevel() >= ui::ZOrderLevel::kFloatingWindow;
}

}  // namespace

// static
OccludedWidgetInputProtector* OccludedWidgetInputProtector::GetInstance() {
  return base::Singleton<OccludedWidgetInputProtector>::get();
}

OccludedWidgetInputProtector::OccludedWidgetInputProtector() = default;

OccludedWidgetInputProtector::~OccludedWidgetInputProtector() = default;

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
