// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/views_export.h"

namespace views {

class Widget;

// This class owns and manages the accessibility tree for a Widget. It is owned
// by the `widget_` and must never outlive its owner. This is currently under
// construction.
class VIEWS_EXPORT WidgetAXManager {
 public:
  explicit WidgetAXManager(Widget* widget);
  WidgetAXManager(const WidgetAXManager&) = delete;
  WidgetAXManager& operator=(const WidgetAXManager&) = delete;
  ~WidgetAXManager();

  void Enable();
  void Disable();

  bool is_enabled() const { return is_enabled_; }

 private:
  // The widget this manager is owned by.
  raw_ptr<Widget> widget_;

  // Indicates whether we're actively serializing widget accessibility data.
  bool is_enabled_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_
