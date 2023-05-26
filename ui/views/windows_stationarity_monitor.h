// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOWS_STATIONARITY_MONITOR_H_
#define UI_VIEWS_WINDOWS_STATIONARITY_MONITOR_H_

#include "base/observer_list.h"
#include "ui/views/views_export.h"

namespace views {

// A singleton class used to track the stationary state of a list Windows and
// notify registered `InputEventActivationProtector` whenever a tracked window
// has changed its bound or has been closed. Consequently, the input event
// protector should block user input events for a proper delay from that
// timestamp. The tracked list contains native windows which are encapsulated in
// a window host implementation for specific platforms (WindowTreeHost on aura)
// or NativeWidgetMac on Mac. When a window is destroyed, it is removed from the
// tracked list.
class VIEWS_EXPORT WindowsStationarityMonitor {
 public:
  class Observer {
   public:
    // Called before a tracked window is destroyed or changed bounds.
    virtual void OnWindowStationaryStateChanged() {}

   protected:
    virtual ~Observer() = default;
  };

  WindowsStationarityMonitor(const WindowsStationarityMonitor&) = delete;
  WindowsStationarityMonitor& operator=(const WindowsStationarityMonitor&) =
      delete;

  virtual ~WindowsStationarityMonitor();

  // Adds/Removes an observer that will care about whenever the tracked windows
  // have been changed (e.g. a window closed or bounds changed)
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  static WindowsStationarityMonitor* GetInstance();

 protected:
  WindowsStationarityMonitor();

  void NotifyWindowStationaryStateChanged();

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace views

#endif  // UI_VIEWS_WINDOWS_STATIONARITY_MONITOR_H_
