// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_
#define UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_

#include <vector>

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

  DisplayChangeNotifier(const DisplayChangeNotifier&) = delete;
  DisplayChangeNotifier& operator=(const DisplayChangeNotifier&) = delete;

  ~DisplayChangeNotifier();

  void AddObserver(DisplayObserver* observer);

  void RemoveObserver(DisplayObserver* observer);

  void NotifyDisplaysChanged(const std::vector<Display>& old_displays,
                             const std::vector<Display>& new_displays);

  void NotifyCurrentWorkspaceChanged(const std::string& workspace);

 private:
  // The observers that need to be notified when a display is modified, added
  // or removed.
  base::ObserverList<DisplayObserver> observer_list_;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_
