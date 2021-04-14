// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_event_manager.h"

#include "base/no_destructor.h"
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
  for (AXEventObserver& observer : observers_)
    observer.OnViewEvent(view, event_type);
}

}  // namespace views
