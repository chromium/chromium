// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_X11_TOUCH_FACTORY_X11_H_
#define UI_EVENTS_DEVICES_X11_TOUCH_FACTORY_X11_H_

#include <stdint.h>

#include <bitset>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "ui/events/devices/x11/events_devices_x11_export.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/gfx/x/x11_types.h"

namespace base {

template <typename T> struct DefaultSingletonTraits;
}

typedef unsigned long Cursor;
typedef unsigned long Window;

namespace ui {

// Functions related to determining touch devices.
class EVENTS_DEVICES_X11_EXPORT TouchFactory {
 private:
  TouchFactory();
  ~TouchFactory();

 public:
  // Returns the TouchFactory singleton.
  static TouchFactory* GetInstance();

  // Sets the touch devices from the command line.
  static void SetTouchDeviceListFromCommandLine();

  // Updates the list of devices.
  void UpdateDeviceList(XDisplay* display);

  // Checks whether an XI2 event should be processed or not (i.e. if the event
  // originated from a device we are interested in).
  bool ShouldProcessXI2Event(XEvent* xevent);

  // Setup an X Window for XInput2 events.
  void SetupXI2ForXWindow(::Window xid);

  // Keeps a list of touch devices so that it is possible to determine if a
  // pointer event is a touch-event or a mouse-event. The list is reset each
  // time this is called.
  void SetTouchDeviceList(
      const std::vector<std::pair<int, EventPointerType>>& devices);

  // Is the device ID valid?
  bool IsValidDevice(int deviceid) const;

  // Is the device a touch-device?
  bool IsTouchDevice(int deviceid) const;

  // Is the device a real multi-touch-device? (see doc. for |touch_device_list_|
  // below for more explanation.)
  bool IsMultiTouchDevice(int deviceid) const;

  // Gets the pointer type for touch-device.
  EventPointerType GetTouchDevicePointerType(int deviceid) const;

  // Tries to find an existing slot ID mapping to tracking ID. Returns true
  // if the slot is found and it is saved in |slot|, false if no such slot
  // can be found.
  bool QuerySlotForTrackingID(uint32_t tracking_id, int* slot);

  // Tries to find an existing slot ID mapping to tracking ID. If there
  // isn't one already, allocates a new slot ID and sets up the mapping.
  int GetSlotForTrackingID(uint32_t tracking_id);

  // Releases the slot ID mapping to tracking ID.
  void ReleaseSlot(int slot);

  // Whether any touch device is currently present and enabled.
  bool IsTouchDevicePresent();

  // Pairs of <vendor id, product id> of external touch screens.
  const std::set<std::pair<int, int> >& GetTouchscreenIds() const {
    return touchscreen_ids_;
  }

  // Resets the TouchFactory singleton.
  void ResetForTest();

  // Sets up the device id in the list |devices| as multi-touch capable
  // devices and enables touch events processing. This function is only
  // for test purpose, and it does not query from X server.
  void SetTouchDeviceForTest(const std::vector<int>& devices);

  // Sets up the device id in the list |devices| as pointer devices.
  // This function is only for test purpose, and it does not query from
  // X server.
  void SetPointerDeviceForTest(const std::vector<int>& devices);

  // Sets the status of the touch screens to |enabled|.
  void SetTouchscreensEnabled(bool enabled);

 private:
  // Requirement for Singleton
  friend struct base::DefaultSingletonTraits<TouchFactory>;

  void CacheTouchscreenIds(int id);

  // NOTE: To keep track of touch devices, we currently maintain a lookup table
  // to quickly decide if a device is a touch device or not. We also maintain a
  // list of the touch devices. Ideally, there will be only one touch device,
  // and instead of having the lookup table and the list, there will be a single
  // identifier for the touch device. This can be completed after enough testing
  // on real touch devices.

  static const int kMaxDeviceNum = 128;

  // A quick lookup table for determining if events from the pointer device
  // should be processed.
  std::bitset<kMaxDeviceNum> pointer_device_lookup_;

  // A quick lookup table for determining if a device is a touch device.
  std::bitset<kMaxDeviceNum> touch_device_lookup_;

  // The list of touch devices. For testing/debugging purposes, a single-pointer
  // device (mouse or touch screen without sufficient X/driver support for MT)
  // can sometimes be treated as a touch device. The key in the map represents
  // the device id, and the value contains the details for device (e.g. if the
  // device is master and multi-touch capable).
  struct TouchDeviceDetails {
    bool is_master = false;
    EventPointerType pointer_type = EventPointerType::POINTER_TYPE_TOUCH;
  };
  std::map<int, TouchDeviceDetails> touch_device_list_;

  // Touch screen <vid, pid>s.
  std::set<std::pair<int, int> > touchscreen_ids_;

  // Device ID of the virtual core keyboard.
  int virtual_core_keyboard_device_;

  SequentialIDGenerator id_generator_;

  // Associate each device ID with its master device ID.
  std::map<int, int> device_master_id_list_;

  // The status of the touch screens devices themselves.
  bool touch_screens_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TouchFactory);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_X11_TOUCH_FACTORY_X11_H_
