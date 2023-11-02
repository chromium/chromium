// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_LIST_H_
#define UI_DISPLAY_DISPLAY_LIST_H_

#include <stdint.h>

#include <vector>

#include "base/observer_list.h"
#include "ui/display/display.h"
#include "ui/display/display_export.h"

namespace display {

class DisplayObserver;

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

  DisplayList(const DisplayList&) = delete;
  DisplayList& operator=(const DisplayList&) = delete;

  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  const Displays& displays() const { return displays_; }

  Displays::const_iterator FindDisplayById(int64_t id) const;

  // Get an iterator for the primary display. This returns an invalid iterator
  // if no such display is available. Callers must check the returned value
  // against `displays().end()` before dereferencing.
  Displays::const_iterator GetPrimaryDisplayIterator() const;

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

  // Checks for general struct validity. This permits empty lists, but non-empty
  // lists must specify a primary display and the displays must not use repeated
  // or invalid ids.
  bool IsValid() const;

  base::ObserverList<DisplayObserver>* observers() { return &observers_; }
  const base::ObserverList<DisplayObserver>* observers() const {
    return &observers_;
  }

 private:
  // A non-const version of FindDisplayById.
  Displays::iterator FindDisplayByIdInternal(int64_t id);

  // The list of displays tracked by the display::Screen or other client.
  std::vector<Display> displays_;
  // The id of the primary Display in `displays_` for the display::Screen.
  int64_t primary_id_ = kInvalidDisplayId;
  base::ObserverList<DisplayObserver> observers_;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_LIST_H_
