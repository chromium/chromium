// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ANY_WIDGET_OBSERVER_SINGLETON_H_
#define UI_VIEWS_WIDGET_ANY_WIDGET_OBSERVER_SINGLETON_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace views {

class AnyWidgetObserver;
class Widget;

namespace internal {

// This is not the class you want - go look at AnyWidgetObserver.

// This class serves as the "thing being observed" by AnyWidgetObservers,
// since there is no relevant singleton for Widgets. Every Widget notifies the
// singleton instance of this class of its creation, and it handles notifying
// all AnyWidgetObservers of that.
class AnyWidgetObserverSingleton {
 public:
  static AnyWidgetObserverSingleton* GetInstance();

  void OnAnyWidgetInitialized(Widget* widget);
  void OnAnyWidgetShown(Widget* widget);
  void OnAnyWidgetHidden(Widget* widget);
  void OnAnyWidgetClosing(Widget* widget);

  void AddObserver(AnyWidgetObserver* observer);
  void RemoveObserver(AnyWidgetObserver* observer);

 private:
  friend class base::NoDestructor<AnyWidgetObserverSingleton>;

  AnyWidgetObserverSingleton();
  ~AnyWidgetObserverSingleton();

  base::ObserverList<AnyWidgetObserver> observers_;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_WIDGET_ANY_WIDGET_OBSERVER_SINGLETON_H_
