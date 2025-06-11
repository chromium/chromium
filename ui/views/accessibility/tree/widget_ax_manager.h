// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/views/views_export.h"

namespace views {

class ViewAccessibility;
class Widget;

// This class owns and manages the accessibility tree for a Widget. It is owned
// by the `widget_` and must never outlive its owner. This is currently under
// construction.
class VIEWS_EXPORT WidgetAXManager : public ui::AXModeObserver {
 public:
  explicit WidgetAXManager(Widget* widget);
  WidgetAXManager(const WidgetAXManager&) = delete;
  WidgetAXManager& operator=(const WidgetAXManager&) = delete;
  ~WidgetAXManager() override;

  void Enable();

  bool is_enabled() const { return is_enabled_; }

  void OnEvent(ViewAccessibility& view_ax, ax::mojom::Event event_type);
  void OnDataChanged(ViewAccessibility& view_ax);

  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;

 private:
  // The widget this manager is owned by.
  raw_ptr<Widget> widget_;

  // Indicates whether we're actively serializing widget accessibility data.
  bool is_enabled_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TREE_WIDGET_AX_MANAGER_H_
