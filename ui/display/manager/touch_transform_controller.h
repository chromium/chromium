// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_TOUCH_TRANSFORM_CONTROLLER_H_
#define UI_DISPLAY_MANAGER_TOUCH_TRANSFORM_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {
struct TouchscreenDevice;
struct TouchDeviceTransform;
}  // namespace ui

namespace display {

class DisplayManager;
class ManagedDisplayInfo;
class TouchTransformSetter;

namespace test {
class TouchTransformControllerTest;
class TouchTransformControllerTestApi;
}  // namespace test

// TouchTransformController matches touchscreen displays with touch
// input-devices and computes the coordinate transforms between display space
// and input-device space.
class DISPLAY_MANAGER_EXPORT TouchTransformController {
 public:
  TouchTransformController(DisplayManager* display_manager,
                           std::unique_ptr<TouchTransformSetter> setter);

  TouchTransformController(const TouchTransformController&) = delete;
  TouchTransformController& operator=(const TouchTransformController&) = delete;

  ~TouchTransformController();

  // Updates the transform for touch input-devices and pushes the new transforms
  // into device manager.
  void UpdateTouchTransforms() const;

  // During touch calibration we remove the previous transform and update touch
  // transformer until calibration is complete.
  void SetForCalibration(bool is_calibrating);

 private:
  friend class test::TouchTransformControllerTest;
  friend class test::TouchTransformControllerTestApi;

  // Contains the data that is passed to TouchTransformSetter.
  struct UpdateData {
    UpdateData();
    ~UpdateData();

    std::map<int, double> device_to_scale;
    std::vector<ui::TouchDeviceTransform> touch_device_transforms;
  };

  void UpdateTouchTransforms(UpdateData* data) const;

  // Returns a transform that will be used to change an event's location from
  // the touchscreen's coordinate system into |display|'s coordinate system.
  // The transform is also responsible for properly scaling the display if the
  // display supports panel fitting.
  //
  // On Ozone events are reported in the touchscreen's resolution, so
  // |touch_display| is used to determine the size and scale the event.
  gfx::Transform GetTouchTransform(
      const ManagedDisplayInfo& display,
      const ManagedDisplayInfo& touch_display,
      const ui::TouchscreenDevice& touchscreen) const;

  // Returns the scaling factor for the touch radius such that it scales the
  // radius from |touch_device|'s coordinate system to the |touch_display|'s
  // coordinate system.
  double GetTouchResolutionScale(
      const ManagedDisplayInfo& touch_display,
      const ui::TouchscreenDevice& touch_device) const;

  // For the provided |display| update the touch radius mapping in
  // |update_data|.
  void UpdateTouchRadius(const ManagedDisplayInfo& display,
                         UpdateData* update_data) const;

  // For a given |target_display| and |target_display_id| update the touch
  // transformation in |update_data| based on the touchscreen associated with
  // |touch_display|. |target_display_id| is the display id to update the
  // transform for.
  // |touch_display| is the physical display that has the touchscreen
  // from which the events arrive.
  // |target_display| provides the dimensions to which the touch event will be
  // transformed.
  void UpdateTouchTransform(int64_t target_display_id,
                            const ManagedDisplayInfo& touch_display,
                            const ManagedDisplayInfo& target_display,
                            UpdateData* update_data) const;

  // |display_manager_| are not owned and must outlive TouchTransformController.
  raw_ptr<DisplayManager> display_manager_;

  bool is_calibrating_ = false;

  std::unique_ptr<TouchTransformSetter> touch_transform_setter_;
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_TOUCH_TRANSFORM_CONTROLLER_H_
