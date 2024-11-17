// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_event_manager.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "ui/views/accessibility/ax_event_observer.h"

namespace views {

AXEventManager::AXEventManager() = default;

AXEventManager::~AXEventManager() = default;

// static
AXEventManager* AXEventManager::Get() {
  static base::NoDestructor<AXEventManager> instance;
  return instance.get();
}

void AXEventManager::AddObserver(AXEventObserver* observer) {
  observers_.AddObserver(observer);
}

void AXEventManager::RemoveObserver(AXEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AXEventManager::NotifyViewEvent(views::View* view,
                                     ax::mojom::Event event_type) {
  observers_.Notify(&AXEventObserver::OnViewEvent, view, event_type);
}

void AXEventManager::NotifyVirtualViewEvent(views::AXVirtualView* virtual_view,
                                            ax::mojom::Event event_type) {
  observers_.Notify(&AXEventObserver::OnVirtualViewEvent, virtual_view,
                    event_type);
}

}  // namespace views
