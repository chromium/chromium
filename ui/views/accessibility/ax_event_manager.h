// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_EVENT_MANAGER_H_
#define UI_VIEWS_ACCESSIBILITY_AX_EVENT_MANAGER_H_

#include "base/observer_list.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/views/views_export.h"

namespace views {

class AXEventObserver;
class AXVirtualView;
class View;

// AXEventManager allows observation of accessibility events for all views.
class VIEWS_EXPORT AXEventManager {
 public:
  AXEventManager();
  AXEventManager(const AXEventManager&) = delete;
  AXEventManager& operator=(const AXEventManager&) = delete;
  ~AXEventManager();

  // Returns the singleton instance.
  static AXEventManager* Get();

  void AddObserver(AXEventObserver* observer);
  void RemoveObserver(AXEventObserver* observer);

  // Notifies observers of an accessibility event. |view| must not be null.
  void NotifyViewEvent(views::View* view, ax::mojom::Event event_type);
  void NotifyVirtualViewEvent(views::AXVirtualView* virtual_view,
                              ax::mojom::Event event_type);

 private:
  base::ObserverList<AXEventObserver> observers_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_EVENT_MANAGER_H_
