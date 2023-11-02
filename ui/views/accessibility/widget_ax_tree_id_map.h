// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_WIDGET_AX_TREE_ID_MAP_H_
#define UI_VIEWS_ACCESSIBILITY_WIDGET_AX_TREE_ID_MAP_H_

#include <map>
#include <vector>

#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/views/views_export.h"

namespace views {
class Widget;

// This class manages mapping between Widgets and their associated AXTreeIDs.
// It is a singleton wrapper around a std::map. Widget pointers are used as the
// key for the map and AXTreeID's are used as the value returned.
class VIEWS_EXPORT WidgetAXTreeIDMap {
 public:
  WidgetAXTreeIDMap();
  ~WidgetAXTreeIDMap();
  static WidgetAXTreeIDMap& GetInstance();

  bool HasWidget(Widget* widget);
  void AddWidget(ui::AXTreeID tree_id, Widget* widget);
  void RemoveWidget(Widget* widget);
  ui::AXTreeID GetWidgetTreeID(views::Widget* widget);
  const std::vector<Widget*> GetWidgets() const;

 private:
  std::map<Widget*, ui::AXTreeID> widget_map_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_WIDGET_AX_TREE_ID_MAP_H_
