// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/hit_test_utils.h"

#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

int GetHitTestComponent(View* view, const gfx::Point& point_in_widget) {
  gfx::Point point_in_view(point_in_widget);
  View::ConvertPointFromWidget(view, &point_in_view);

  if (!view->GetLocalBounds().Contains(point_in_view))
    return HTNOWHERE;

  View* target_view = view->GetEventHandlerForPoint(point_in_view);
  while (target_view) {
    int component = target_view->GetProperty(kHitTestComponentKey);
    if (component != HTNOWHERE)
      return component;
    if (target_view == view)
      break;
    target_view = target_view->parent();
  }

  return HTNOWHERE;
}

void SetHitTestComponent(View* view, int hit_test_id) {
  view->SetProperty(kHitTestComponentKey, hit_test_id);
}

}  // namespace views
