// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_
#define UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/display_export.h"

namespace display {
class Display;
class DisplayObserver;

// DisplayChangeNotifier is a class implementing the handling of DisplayObserver
// notification for Screen.
class DISPLAY_EXPORT DisplayChangeNotifier {
 public:
  DisplayChangeNotifier();
  ~DisplayChangeNotifier();

  void AddObserver(DisplayObserver* observer);

  void RemoveObserver(DisplayObserver* observer);

  void NotifyDisplaysChanged(const std::vector<Display>& old_displays,
                             const std::vector<Display>& new_displays);

 private:
  // The observers that need to be notified when a display is modified, added
  // or removed.
  base::ObserverList<DisplayObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DisplayChangeNotifier);
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_
