// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_UPDATE_NOTIFIER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_UPDATE_NOTIFIER_H_

#include "base/observer_list.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/views/views_export.h"

namespace views {

class AXUpdateObserver;
class AXVirtualView;
class View;
class ViewAccessibility;

// AXUpdateNotifier allows observation of accessibility updates for all views
// and notifies per-widget accessibility managers of changes.
class VIEWS_EXPORT AXUpdateNotifier {
 public:
  AXUpdateNotifier();
  AXUpdateNotifier(const AXUpdateNotifier&) = delete;
  AXUpdateNotifier& operator=(const AXUpdateNotifier&) = delete;
  ~AXUpdateNotifier();

  // Returns the singleton instance.
  static AXUpdateNotifier* Get();

  void AddObserver(AXUpdateObserver* observer);
  void RemoveObserver(AXUpdateObserver* observer);

  // Notifies observers of an accessibility event. |view| must not be null.
  void NotifyViewEvent(views::View* view, ax::mojom::Event event_type);
  void NotifyVirtualViewEvent(views::AXVirtualView* virtual_view,
                              ax::mojom::Event event_type);

  // Notifies observers of a data change. `view` must not be null.
  void NotifyViewDataChanged(views::View* view);
  void NotifyVirtualViewDataChanged(views::AXVirtualView* virtual_view);

  void NotifyChildAdded(views::ViewAccessibility* child,
                        views::ViewAccessibility* parent);
  void NotifyChildRemoved(views::ViewAccessibility* child,
                          views::ViewAccessibility* parent);

 private:
  base::ObserverList<AXUpdateObserver> observers_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_UPDATE_NOTIFIER_H_
