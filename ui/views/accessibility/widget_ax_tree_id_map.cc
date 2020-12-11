// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/widget_ax_tree_id_map.h"

#include "base/stl_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace views {

WidgetAXTreeIDMap::WidgetAXTreeIDMap() {}

WidgetAXTreeIDMap::~WidgetAXTreeIDMap() {}

// static
WidgetAXTreeIDMap& WidgetAXTreeIDMap::GetInstance() {
  static base::NoDestructor<WidgetAXTreeIDMap> instance;
  return *instance;
}

bool WidgetAXTreeIDMap::HasWidget(Widget* widget) {
  return base::Contains(widget_map_, widget);
}

void WidgetAXTreeIDMap::AddWidget(ui::AXTreeID tree_id, Widget* widget) {
  DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
  DCHECK(widget);
  DCHECK(!HasWidget(widget));
  widget_map_[widget] = tree_id;
}

void WidgetAXTreeIDMap::RemoveWidget(Widget* widget) {
  widget_map_.erase(widget);
}

ui::AXTreeID WidgetAXTreeIDMap::GetWidgetTreeID(Widget* widget) {
  DCHECK(widget);
  if (!base::Contains(widget_map_, widget))
    return ui::AXTreeIDUnknown();

  return widget_map_.at(widget);
}

const std::vector<Widget*> WidgetAXTreeIDMap::GetWidgets() const {
  std::vector<Widget*> widgets;
  widgets.reserve(widget_map_.size());

  for (auto iter : widget_map_)
    widgets.push_back(iter.first);

  return widgets;
}

}  // namespace views
