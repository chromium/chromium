// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_UPDATE_OBSERVER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_UPDATE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/views/views_export.h"

namespace views {

class AXVirtualView;
class View;
class ViewAccessibility;

// AXUpdateObserver is notified for accessibility events and data changes on all
// views.
class VIEWS_EXPORT AXUpdateObserver : public base::CheckedObserver {
 public:
  virtual void OnViewEvent(views::View* view, ax::mojom::Event event_type) = 0;
  virtual void OnVirtualViewEvent(views::AXVirtualView* virtual_view,
                                  ax::mojom::Event event_type) {}

  virtual void OnDataChanged(views::View* view) {}
  virtual void OnVirtualViewDataChanged(views::AXVirtualView* virtual_view) {}

  virtual void OnChildAdded(views::ViewAccessibility* child,
                            views::ViewAccessibility* parent) {}
  virtual void OnChildRemoved(views::ViewAccessibility* child,
                              views::ViewAccessibility* parent) {}

 protected:
  AXUpdateObserver();
  ~AXUpdateObserver() override;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_UPDATE_OBSERVER_H_
