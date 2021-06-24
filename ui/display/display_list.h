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

  // WARNING: The copy constructor and assignment operator do not copy nor move
  // observers; also, the comparison operator does not compare observers.
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

  // Get an iterator for the primary display. This returns an invalid iterator
  // if no such display is available. Callers must check the returned value
  // against `displays().end()` before dereferencing.
  Displays::const_iterator GetPrimaryDisplayIterator() const;

  // Get a reference to the primary or current display. This will CHECK if no
  // such display is available. Callers must know that the display is available.
  const Display& GetPrimaryDisplay() const;
  const Display& GetCurrentDisplay() const;

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

  // Checks for general struct validity. This permits empty lists, and the
  // current display may be unspecified, but non-empty lists must specify a
  // primary display and the displays must not use repeated id values.
  bool IsValidOrEmpty() const;

  // Checks for validity, and for the presence of primary and current displays.
  // This is a stronger check than IsValid, which allows the list to be empty.
  bool IsValidAndHasPrimaryAndCurrentDisplays() const;

  base::ObserverList<DisplayObserver>* observers() { return &observers_; }

 private:
  Type GetTypeByDisplayId(int64_t display_id) const;

  Displays::iterator FindDisplayByIdInternal(int64_t id);

  // The list of displays tracked by the display::Screen or other client.
  std::vector<Display> displays_;
  // The id of the primary Display in `displays_` for the display::Screen.
  int64_t primary_id_ = kInvalidDisplayId;
  // The id of the current Display in `displays_`, for some client's context.
  // This is used when DisplayList needs to track which display a client is on,
  // typically for a cached DisplayList owned by the client window itself. This
  // should be kInvalidDisplayId for the DisplayList owned by display::Screen,
  // which represents the system-wide state and needs to work for all clients.
  // This member is included in this structure to maintain consistency with some
  // list of cached displays, perhaps that's not ideal. See crbug.com/1207996.
  int64_t current_id_ = kInvalidDisplayId;
  base::ObserverList<DisplayObserver> observers_;
};

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_LIST_H_
