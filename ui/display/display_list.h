// Copyright 2016 The Chromium Authors. All rights reserved.
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

  // WARNING: These constructors and operators do not consider observers.
  DisplayList(const Displays& displays, int64_t primary_id, int64_t current_id);
  DisplayList(const DisplayList& other);
  DisplayList& operator=(const DisplayList& other);
  bool operator==(const DisplayList& other) const;

  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  const Displays& displays() const { return displays_; }
  int64_t primary_id() const { return primary_id_; }
  int64_t current_id() const { return current_id_; }

  Displays::const_iterator FindDisplayById(int64_t id) const;

  Displays::const_iterator GetPrimaryDisplayIterator() const;
  Displays::const_iterator GetCurrentDisplayIterator() const;

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

  // Checks expectations around DisplayList validity.
  bool IsValid() const;

  base::ObserverList<DisplayObserver>* observers() { return &observers_; }

 private:
  Type GetTypeByDisplayId(int64_t display_id) const;

  Displays::iterator FindDisplayByIdInternal(int64_t id);

  std::vector<Display> displays_;
  int64_t primary_id_ = kInvalidDisplayId;
  int64_t current_id_ = kInvalidDisplayId;
  base::ObserverList<DisplayObserver> observers_;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_LIST_H_
