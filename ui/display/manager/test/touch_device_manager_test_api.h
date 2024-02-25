// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TEST_TOUCH_DEVICE_MANAGER_TEST_API_H_
#define UI_DISPLAY_MANAGER_TEST_TOUCH_DEVICE_MANAGER_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/manager/touch_device_manager.h"

namespace ui {
struct TouchscreenDevice;
}  // namespace ui

namespace display {

class ManagedDisplayInfo;

namespace test {

class DISPLAY_MANAGER_EXPORT TouchDeviceManagerTestApi {
 public:
  explicit TouchDeviceManagerTestApi(TouchDeviceManager* touch_device_manager);

  TouchDeviceManagerTestApi(const TouchDeviceManagerTestApi&) = delete;
  TouchDeviceManagerTestApi& operator=(const TouchDeviceManagerTestApi&) =
      delete;

  ~TouchDeviceManagerTestApi();

  // Associate the given display |display_info| with the touch device |device|.
  void Associate(ManagedDisplayInfo* display_info,
                 const ui::TouchscreenDevice& device);

  // Associate the given display identified by |display_id| with the touch
  // device |device|.
  void Associate(int64_t display_id, const ui::TouchscreenDevice& device);

  // Returns the count of touch devices currently associated with the display
  // |info|.
  std::size_t GetTouchDeviceCount(const ManagedDisplayInfo& info) const;

  // Returns true if the display |info| and touch device |device| are currently
  // associated.
  bool AreAssociated(const ManagedDisplayInfo& info,
                     const ui::TouchscreenDevice& device) const;

  // Resets the touch device manager by clearing all records of historical
  // touch association and calibration data.
  void ResetTouchDeviceManager();

 private:
  // Not owned
  raw_ptr<TouchDeviceManager> touch_device_manager_;
};

}  // namespace test

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TEST_TOUCH_DEVICE_MANAGER_TEST_API_H_
