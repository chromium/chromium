// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_
#define UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_

#include <stdint.h>

#include <vector>

#include "base/observer_list.h"
#include "build/buildflag.h"
#include "ui/display/display_export.h"
#include "ui/display/types/display_constants.h"

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

  // Notifies observers of displays that were added, removed or changed
  // between `old_displays` and `new_displays`. If `old_primary_id` and
  // `new_primary_id` differ, the display that became primary is reported
  // with `DisplayObserver::DISPLAY_METRIC_PRIMARY`, unless it was just added.
  void NotifyDisplaysChanged(const std::vector<Display>& old_displays,
                             const std::vector<Display>& new_displays,
                             int64_t old_primary_id = kInvalidDisplayId,
                             int64_t new_primary_id = kInvalidDisplayId);

  void NotifyCurrentWorkspaceChanged(const std::string& workspace);

#if BUILDFLAG(IS_MAC)
  void NotifyPrimaryDisplayChanged();
#endif

 private:
  // The observers that need to be notified when a display is modified, added
  // or removed.
  base::ObserverList<DisplayObserver> observer_list_;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_CHANGE_NOTIFIER_H_
