// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/any_widget_observer_singleton.h"
#include "ui/views/widget/any_widget_observer.h"

#include "base/no_destructor.h"

namespace views {
namespace internal {

// static
AnyWidgetObserverSingleton* AnyWidgetObserverSingleton::GetInstance() {
  static base::NoDestructor<AnyWidgetObserverSingleton> observer;
  return observer.get();
}

#define PROPAGATE_NOTIFICATION(method)                      \
  void AnyWidgetObserverSingleton::method(Widget* widget) { \
    for (AnyWidgetObserver & obs : observers_)              \
      obs.method(widget);                                   \
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

}  // namespace internal
}  // namespace views
