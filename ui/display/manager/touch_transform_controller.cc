// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/touch_transform_controller.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/manager/touch_transform_setter.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/gfx/geometry/transform.h"

namespace display {

namespace {

// Given an array of touch point and display point pairs, this function computes
// and returns the constants(defined below) using a least fit algorithm.
// If (xt, yt) is a touch point then its corresponding (xd, yd) would be defined
// by the following 2 equations:
// xd = xt * A + yt * B + C
// yd = xt * D + yt * E + F
// This function computes A, B, C, D, E and F and sets |ctm| with the calibrated
// transform matrix. In case the computation fails, the function will return
// false.
// See http://crbug.com/672293
bool GetCalibratedTransform(
    std::array<std::pair<gfx::Point, gfx::Point>, 4> touch_point_pairs,
    const gfx::Transform& pre_calibration_tm,
    gfx::Transform* ctm) {
  // Transform the display points before solving the equation.
  // If the calibration was performed at a resolution that is 0.5 times the
  // current resolution, then the display points (x, y) for a given touch point
  // now represents a display point at (2 * x, 2 * y). This and other kinds of
  // similar transforms can be applied using |pre_calibration_tm|.
  for (int row = 0; row < 4; row++) {
    touch_point_pairs[row].first =
        pre_calibration_tm.MapPoint(touch_point_pairs[row].first);
  }

  // Vector of the X-coordinate of display points corresponding to each of the
  // touch points.
  float display_points_x[4] = {
      static_cast<float>(touch_point_pairs[0].first.x()),
      static_cast<float>(touch_point_pairs[1].first.x()),
      static_cast<float>(touch_point_pairs[2].first.x()),
      static_cast<float>(touch_point_pairs[3].first.x())};
  // Vector of the Y-coordinate of display points corresponding to each of the
  // touch points.
  float display_points_y[4] = {
      static_cast<float>(touch_point_pairs[0].first.y()),
      static_cast<float>(touch_point_pairs[1].first.y()),
      static_cast<float>(touch_point_pairs[2].first.y()),
      static_cast<float>(touch_point_pairs[3].first.y())};

  // Initialize |touch_point_matrix|
  // If {(xt_1, yt_1), (xt_2, yt_2), (xt_3, yt_3)....} are a set of touch points
  // received during calibration, then the |touch_point_matrix| would be defined
  // as:
  // |xt_1  yt_1  1  0|
  // |xt_2  yt_2  1  0|
  // |xt_3  yt_3  1  0|
  // |xt_4  yt_4  1  0|
  gfx::Transform touch_point_matrix;
  for (int row = 0; row < 4; row++) {
    touch_point_matrix.set_rc(row, 0, touch_point_pairs[row].second.x());
    touch_point_matrix.set_rc(row, 1, touch_point_pairs[row].second.y());
    touch_point_matrix.set_rc(row, 2, 1);
    touch_point_matrix.set_rc(row, 3, 0);
  }
  gfx::Transform touch_point_matrix_transpose = touch_point_matrix;
  touch_point_matrix_transpose.Transpose();

  gfx::Transform product_matrix =
      touch_point_matrix_transpose * touch_point_matrix;

  // Set (3, 3) = 1 so that |determinant| of the matrix is != 0 and the inverse
  // can be calculated.
  product_matrix.set_rc(3, 3, 1);

  gfx::Transform product_matrix_inverse;

  // NOTE: If the determinant is zero then the inverse cannot be computed. The
  // only solution is to restart touch calibration and get new points from user.
  if (!product_matrix.GetInverse(&product_matrix_inverse)) {
    NOTREACHED_IN_MIGRATION()
        << "Touch Calibration failed. Determinant is zero.";
    return false;
  }

  product_matrix_inverse.set_rc(3, 3, 0);

  product_matrix = product_matrix_inverse * touch_point_matrix_transpose;

  // The result [A, B, C, 0] will be used to calibrate the x-coordinate of
  // touch input:
  //   x_new = x_old * A + y_old * B + C;
  product_matrix.TransformVector4(display_points_x);
  // The result [D, E, F, 0] will be used to calibrate the y-coordinate of
  // touch input:
  //   y_new = x_old * D + y_old * E + F;
  product_matrix.TransformVector4(display_points_y);

  // Create a transform matrix using the touch calibration data.
  // clang-format off
  ctm->PostConcat(gfx::Transform::RowMajor(
      display_points_x[0], display_points_x[1], 0, display_points_x[2],
      display_points_y[0], display_points_y[1], 0, display_points_y[2],
      0, 0, 1, 0,
      0, 0, 0, 1));
  // clang-format on
  return true;
}

// Returns an uncalibrated touch transform.
gfx::Transform GetUncalibratedTransform(const gfx::Transform& tm,
                                        const ManagedDisplayInfo& display,
                                        const ManagedDisplayInfo& touch_display,
                                        const gfx::SizeF& touch_area,
                                        const gfx::SizeF& touch_native_size) {
  gfx::SizeF current_size(display.bounds_in_native().size());
  gfx::Transform ctm(tm);
  // Take care of panel fitting only if supported. Panel fitting is emulated
  // in software mirroring mode (display != touch_display).
  // If panel fitting is enabled then the aspect ratio is preserved and the
  // display is scaled accordingly. In this case blank regions would be present
  // in order to center the displayed area.
  if (display.is_aspect_preserving_scaling() ||
      display.id() != touch_display.id()) {
    float touch_calib_ar =
        touch_native_size.width() / touch_native_size.height();
    float current_ar = current_size.width() / current_size.height();

    if (current_ar > touch_calib_ar) {  // Letterboxing
      ctm.Translate(
          0, (1 - current_ar / touch_calib_ar) * 0.5 * current_size.height());
      ctm.Scale(1, current_ar / touch_calib_ar);
    } else if (touch_calib_ar > current_ar) {  // Pillarboxing
      ctm.Translate(
          (1 - touch_calib_ar / current_ar) * 0.5 * current_size.width(), 0);
      ctm.Scale(touch_calib_ar / current_ar, 1);
    }
  }
  // Take care of scaling between touchscreen area and display resolution.
  ctm.Scale(current_size.width() / touch_area.width(),
            current_size.height() / touch_area.height());
  return ctm;
}

DisplayIdList GetConnectedDisplayIdList(const DisplayManager* display_manager) {
  DCHECK(display_manager->num_connected_displays());
  if (display_manager->num_connected_displays() == 1)
    return DisplayIdList{display_manager->first_display_id()};
  return display_manager->GetConnectedDisplayIdList();
}

}  // namespace

TouchTransformController::UpdateData::UpdateData() = default;

TouchTransformController::UpdateData::~UpdateData() = default;

// This is to compute the scale ratio for the TouchEvent's radius. The
// configured resolution of the display is not always the same as the touch
// screen's reporting resolution, e.g. the display could be set as
// 1920x1080 while the touchscreen is reporting touch position range at
// 32767x32767. Touch radius is reported in the units the same as touch position
// so we need to scale the touch radius to be compatible with the display's
// resolution. We compute the scale as
// sqrt of (display_area / touchscreen_area)
double TouchTransformController::GetTouchResolutionScale(
    const ManagedDisplayInfo& touch_display,
    const ui::TouchscreenDevice& touch_device) const {
  if (touch_device.id == ui::InputDevice::kInvalidId ||
      touch_device.size.IsEmpty() ||
      touch_display.bounds_in_native().size().IsEmpty())
    return 1.0;

  double display_area = touch_display.bounds_in_native().size().Area64();
  double touch_area = touch_device.size.Area64();
  double ratio = std::sqrt(display_area / touch_area);

  VLOG(2) << "Display size: "
          << touch_display.bounds_in_native().size().ToString()
          << ", Touchscreen size: " << touch_device.size.ToString()
          << ", Touch radius scale ratio: " << ratio;
  return ratio;
}

gfx::Transform TouchTransformController::GetTouchTransform(
    const ManagedDisplayInfo& display,
    const ManagedDisplayInfo& touch_display,
    const ui::TouchscreenDevice& touchscreen) const {
  auto current_size = gfx::SizeF(display.bounds_in_native().size());
  auto touch_native_size = gfx::SizeF(touch_display.GetNativeModeSize());
  auto touch_area = gfx::SizeF(touchscreen.size);

  gfx::Transform ctm;

  if (current_size.IsEmpty() || touch_native_size.IsEmpty() ||
      touch_area.IsEmpty() || touchscreen.id == ui::InputDevice::kInvalidId)
    return ctm;

  // Translate the touch so that it falls within the display bounds. This
  // should not be performed if the displays are mirrored.
  if (display.id() == touch_display.id()) {
    ctm.Translate(display.bounds_in_native().x(),
                  display.bounds_in_native().y());
  }

  // If the device is currently under calibration, then do not return any
  // transform as we want to use the raw native touch input data for calibration
  if (is_calibrating_)
    return ctm;

  TouchCalibrationData calibration_data =
      display_manager_->touch_device_manager()->GetCalibrationData(
          touchscreen, touch_display.id());
  // If touch calibration data is unavailable, use naive approach.
  if (calibration_data.IsEmpty()) {
    return GetUncalibratedTransform(ctm, display, touch_display, touch_area,
                                    touch_native_size);
  }

  // The resolution at which the touch calibration was performed.
  gfx::SizeF touch_calib_size(calibration_data.bounds);

  // Any additional transformation that needs to be applied to the display
  // points, before we solve for the final transform.
  gfx::Transform pre_transform;

  if (display.id() != touch_display.id() ||
      display.is_aspect_preserving_scaling()) {
    // Case of displays being mirrored or in panel fitting mode.
    // Aspect ratio of the touch display's resolution during calibration.
    float calib_ar = touch_calib_size.width() / touch_calib_size.height();
    // Aspect ratio of the display that is being mirrored.
    float current_ar = current_size.width() / current_size.height();

    if (current_ar < calib_ar) {
      pre_transform.Scale(current_size.height() / touch_calib_size.height(),
                          current_size.height() / touch_calib_size.height());
      pre_transform.Translate(
          (current_ar / calib_ar - 1.f) * touch_calib_size.width() * 0.5f, 0);
    } else {
      pre_transform.Scale(current_size.width() / touch_calib_size.width(),
                          current_size.width() / touch_calib_size.width());
      pre_transform.Translate(
          0, (calib_ar / current_ar - 1.f) * touch_calib_size.height() * 0.5f);
    }
  } else {
    // Case of current resolution being different from the resolution when the
    // touch calibration was performed.
    pre_transform.Scale(current_size.width() / touch_calib_size.width(),
                        current_size.height() / touch_calib_size.height());
  }
  // Solve for coefficients and compute transform matrix.
  gfx::Transform stored_ctm;
  if (!GetCalibratedTransform(calibration_data.point_pairs, pre_transform,
                              &stored_ctm)) {
    // TODO(malaykeshav): This can be checked at the calibration step before
    // storing the calibration associated data. This will allow us to explicitly
    // inform the user with proper UX.

    // Return uncalibrated transform.
    return GetUncalibratedTransform(ctm, display, touch_display, touch_area,
                                    touch_native_size);
  }

  stored_ctm.PostConcat(ctm);
  return stored_ctm;
}

TouchTransformController::TouchTransformController(
    DisplayManager* display_manager,
    std::unique_ptr<TouchTransformSetter> setter)
    : display_manager_(display_manager),
      touch_transform_setter_(std::move(setter)) {}

TouchTransformController::~TouchTransformController() {}

void TouchTransformController::UpdateTouchTransforms() const {
  UpdateData update_data;
  UpdateTouchTransforms(&update_data);
  touch_transform_setter_->ConfigureTouchDevices(
      update_data.touch_device_transforms);
}

void TouchTransformController::UpdateTouchRadius(
    const ManagedDisplayInfo& display,
    UpdateData* update_data) const {
  for (const auto& device :
       display_manager_->touch_device_manager()
           ->GetAssociatedTouchDevicesForDisplay(display.id())) {
    DCHECK_EQ(0u, update_data->device_to_scale.count(device.id));
    update_data->device_to_scale.emplace(
        device.id, GetTouchResolutionScale(display, device));
  }
}

void TouchTransformController::UpdateTouchTransform(
    int64_t target_display_id,
    const ManagedDisplayInfo& touch_display,
    const ManagedDisplayInfo& target_display,
    UpdateData* update_data) const {
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = target_display_id;
  for (const auto& device :
       display_manager_->touch_device_manager()
           ->GetAssociatedTouchDevicesForDisplay(touch_display.id())) {
    touch_device_transform.device_id = device.id;
    touch_device_transform.transform =
        GetTouchTransform(target_display, touch_display, device);
    auto device_to_scale_iter = update_data->device_to_scale.find(device.id);
    if (device_to_scale_iter != update_data->device_to_scale.end())
      touch_device_transform.radius_scale = device_to_scale_iter->second;
    update_data->touch_device_transforms.push_back(touch_device_transform);
  }
}

void TouchTransformController::UpdateTouchTransforms(
    UpdateData* update_data) const {
  if (display_manager_->num_connected_displays() == 0)
    return;

  DisplayIdList display_id_list = GetConnectedDisplayIdList(display_manager_);
  DCHECK(display_id_list.size());

  DisplayInfoList display_info_list;

  for (int64_t display_id : display_id_list) {
    DCHECK(display_id != kInvalidDisplayId);
    display_info_list.push_back(display_manager_->GetDisplayInfo(display_id));
    UpdateTouchRadius(display_info_list.back(), update_data);
  }

  if (display_manager_->IsInMirrorMode()) {
    std::size_t primary_display_id_index = std::distance(
        display_id_list.begin(),
        base::ranges::find(display_id_list,
                           Screen::GetScreen()->GetPrimaryDisplay().id()));

    for (std::size_t index = 0; index < display_id_list.size(); index++) {
      // In extended but software mirroring mode, there is a WindowTreeHost
      // for each display, but all touches are forwarded to the primary root
      // window's WindowTreeHost.
      // In mirror mode, there is just one WindowTreeHost and two displays.
      // Make the WindowTreeHost accept touch events from both displays.
      std::size_t touch_display_index =
          display_manager_->SoftwareMirroringEnabled()
              ? primary_display_id_index
              : index;
      UpdateTouchTransform(display_id_list[primary_display_id_index],
                           display_info_list[index],
                           display_info_list[touch_display_index], update_data);
    }
    return;
  }

  for (std::size_t index = 0; index < display_id_list.size(); index++) {
    UpdateTouchTransform(display_id_list[index], display_info_list[index],
                         display_info_list[index], update_data);
  }
}

void TouchTransformController::SetForCalibration(bool is_calibrating) {
  is_calibrating_ = is_calibrating;
  UpdateTouchTransforms();
}

}  // namespace display
