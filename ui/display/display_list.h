// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_LIST_H_
#define UI_DISPLAY_DISPLAY_LIST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/display_export.h"

namespace display {

class Display;
class DisplayList;
class DisplayObserver;

// See description in DisplayLock::SuspendObserverUpdates.
class DISPLAY_EXPORT DisplayListObserverLock {
 public:
  ~DisplayListObserverLock();

 private:
  friend class DisplayList;

  explicit DisplayListObserverLock(DisplayList* display_list);

  DisplayList* display_list_;

  DISALLOW_COPY_AND_ASSIGN(DisplayListObserverLock);
};

// Maintains an ordered list of Displays as well as operations to add, remove
// and update said list. Additionally maintains DisplayObservers and updates
// them as appropriate.
class DISPLAY_EXPORT DisplayList {
 public:
  using Displays = std::vector<Display>;

  enum class Type {
    PRIMARY,
    NOT_PRIMARY,
  };

  DisplayList();
  ~DisplayList();

  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  const Displays& displays() const { return displays_; }

  Displays::const_iterator FindDisplayById(int64_t id) const;

  Displays::const_iterator GetPrimaryDisplayIterator() const;

  // Internally increments a counter that while non-zero results in observers
  // not being called for any changes to the displays. It is assumed once
  // callers release the last lock they call the observers appropriately.
  std::unique_ptr<DisplayListObserverLock> SuspendObserverUpdates();

  void AddOrUpdateDisplay(const Display& display, Type type);

  // Updates the cached display based on display.id(). This returns a bitmask
  // of the changed values suitable for passing to
  // DisplayObserver::OnDisplayMetricsChanged().
  uint32_t UpdateDisplay(const Display& display);

  // Updates the cached display based on display.id(). Also updates the primary
  // display if |type| indicates |display| is the primary display. See single
  // argument version for description of return value.
  uint32_t UpdateDisplay(const Display& display, Type type);

  // Adds a new Display.
  void AddDisplay(const Display& display, Type type);

  // Removes the Display with the specified id.
  void RemoveDisplay(int64_t id);

  base::ObserverList<DisplayObserver>* observers() { return &observers_; }

 private:
  friend class DisplayListObserverLock;

  bool should_notify_observers() const {
    return observer_suspend_lock_count_ == 0;
  }
  void IncrementObserverSuspendLockCount();
  void DecrementObserverSuspendLockCount();

  Type GetTypeByDisplayId(int64_t display_id) const;

  Displays::iterator FindDisplayByIdInternal(int64_t id);

  std::vector<Display> displays_;
  int primary_display_index_ = -1;
  base::ObserverList<DisplayObserver> observers_;

  int observer_suspend_lock_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DisplayList);
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_LIST_H_
