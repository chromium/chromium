// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/any_widget_observer_singleton.h"
#include "ui/views/widget/any_widget_observer.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace views::internal {

// static
AnyWidgetObserverSingleton* AnyWidgetObserverSingleton::GetInstance() {
  static base::NoDestructor<AnyWidgetObserverSingleton> observer;
  return observer.get();
}

#define PROPAGATE_NOTIFICATION(method)                      \
  void AnyWidgetObserverSingleton::method(Widget* widget) { \
    observers_.Notify(&AnyWidgetObserver::method, widget);  \
  }

PROPAGATE_NOTIFICATION(OnAnyWidgetInitialized)
PROPAGATE_NOTIFICATION(OnAnyWidgetShown)
PROPAGATE_NOTIFICATION(OnAnyWidgetHidden)
PROPAGATE_NOTIFICATION(OnAnyWidgetClosing)

#undef PROPAGATE_NOTIFICATION

void AnyWidgetObserverSingleton::AddObserver(AnyWidgetObserver* observer) {
  observers_.AddObserver(observer);
}

void AnyWidgetObserverSingleton::RemoveObserver(AnyWidgetObserver* observer) {
  observers_.RemoveObserver(observer);
}

AnyWidgetObserverSingleton::AnyWidgetObserverSingleton() = default;
AnyWidgetObserverSingleton::~AnyWidgetObserverSingleton() = default;

}  // namespace views::internal
