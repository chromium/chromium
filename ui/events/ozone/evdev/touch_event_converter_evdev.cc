// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include <cmath>
#include <limits>
#include <optional>
#include <queue>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"

#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/false_touch_finder.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter_factory.h"
#include "ui/events/ozone/features.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"

namespace {

const int kMaxTrackingId = 0xffff;  // TRKID_MAX in kernel.
const int kMaxTouchSessionGapInSeconds = 5;
const int kMaxTouchStylusGapInSeconds = 10;
const int kMaxRepeatedTouchGapInSeconds = 2;
const float kRepeatedTouchThresholdInSquareMillimeter = 7.0 * 7.0;
const int kMaxTouchStylusGapInMs = 500;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to match TouchType in enums.xml.
enum class TouchType {
  kNone = 0,
  kFinger = 1,
  kPalm = 2,
  kMaxValue = kPalm,
};

// Convert tilt from [min, min + num_values] to [-90deg, +90deg]
float ScaleTilt(int value, int min_value, int num_values) {
  return 180.f * (value - min_value) / num_values - 90.f;
}

int32_t AbsCodeToMtCode(int32_t code) {
  switch (code) {
    case ABS_X:
      return ABS_MT_POSITION_X;
    case ABS_Y:
      return ABS_MT_POSITION_Y;
    case ABS_PRESSURE:
      return ABS_MT_PRESSURE;
    case ABS_DISTANCE:
      return ABS_MT_DISTANCE;
    default:
      return -1;
  }
}

ui::EventPointerType GetEventPointerType(int tool_code) {
  switch (tool_code) {
    case BTN_TOOL_PEN:
      return ui::EventPointerType::kPen;
    case BTN_TOOL_RUBBER:
      return ui::EventPointerType::kEraser;
    default:
      return ui::EventPointerType::kTouch;
  }
}

// This function calculate the touch_major_scale_ and touch_minor_scale_ from
// resolution.
float GetFingerSizeScale(int32_t finger_size_res, int32_t screen_size_res) {
  // If there is no resolution for both events, we assume they are consistent.
  // Though this is not guaranteed by kernel, we don't have any info to guess.
  // If there is a resolution (units/mm) for touch_major/minor, but not a
  // resolution for screen size. We could not get the scale either as we don't
  // have the dpi.
  if (!finger_size_res || !screen_size_res) {
    return 1.0f;
  }
  return static_cast<float>(screen_size_res) / finger_size_res;
}

const int kTrackingIdForUnusedSlot = -1;

struct SupportedHidrawDevice {
  std::string name;
  uint16_t vendor_id;
  uint16_t product_id;
  ui::HeatmapPalmDetector::ModelId model_id;
  std::optional<ui::HeatmapPalmDetector::CropHeatmap> crop_heatmap;
};

// Returns a list of supported hidraw spi devices.
std::vector<SupportedHidrawDevice> GetSupportedHidrawDevices() {
  return {
      {
          .name = "spi 04F3:4222",
          .vendor_id = 0x04F3,
          .product_id = 0x4222,
          .model_id = ui::HeatmapPalmDetector::ModelId::kRex,
          .crop_heatmap = std::nullopt,
      },
      {
          .name = "quickspi-hid 04F3:4222",
          .vendor_id = 0x04F3,
          .product_id = 0x4222,
          .model_id = ui::HeatmapPalmDetector::ModelId::kRex,
          .crop_heatmap = std::nullopt,
      },
      {
          .name = "hid-hxtp 4858:1002",
          .vendor_id = 0x4858,
          .product_id = 0x1002,
          .model_id = ui::HeatmapPalmDetector::ModelId::kGeralt,
          .crop_heatmap = std::nullopt,
      },
      {
          .name = "hid-hxtp 4858:1003",
          .vendor_id = 0x4858,
          .product_id = 0x1003,
          .model_id = ui::HeatmapPalmDetector::ModelId::kGeralt,
          .crop_heatmap = std::optional<ui::HeatmapPalmDetector::CropHeatmap>({
              .left_crop = 1,
              .right_crop = 1,
          }),
      },
  };
}

ui::HeatmapPalmDetector::ModelId GetHidrawModelId(
    const ui::EventDeviceInfo& info) {
  // Do not initialize hidraw device for stylus devices.
  if (info.HasKeyEvent(BTN_TOOL_PEN)) {
    return ui::HeatmapPalmDetector::ModelId::kNotSupported;
  }
  std::vector<SupportedHidrawDevice> supported_hidraw_devices =
      GetSupportedHidrawDevices();
  for (const SupportedHidrawDevice& device : GetSupportedHidrawDevices()) {
    if (info.name() == device.name && info.vendor_id() == device.vendor_id &&
        info.product_id() == device.product_id) {
      return device.model_id;
    }
  }
  return ui::HeatmapPalmDetector::ModelId::kNotSupported;
}

base::FilePath GetHidrawPath(const base::FilePath& root_path) {
  return base::FileEnumerator(root_path, false,
                              base::FileEnumerator::DIRECTORIES)
      .Next();
}

std::optional<ui::HeatmapPalmDetector::CropHeatmap> GetCropHeatmap(
    const ui::EventDeviceInfo& info) {
  // Stylus devices have no heatmap and thus no cropping.
  if (info.HasKeyEvent(BTN_TOOL_PEN)) {
    return std::nullopt;
  }
  std::vector<SupportedHidrawDevice> supported_hidraw_devices =
      GetSupportedHidrawDevices();
  for (const SupportedHidrawDevice& device : GetSupportedHidrawDevices()) {
    if (info.name() == device.name && info.vendor_id() == device.vendor_id &&
        info.product_id() == device.product_id) {
      return device.crop_heatmap;
    }
  }
  return std::nullopt;
}

}  // namespace

namespace ui {

BASE_FEATURE(kEnableSingleCancelTouch,
             "EnableSingleTouchCancel",
             base::FEATURE_DISABLED_BY_DEFAULT);

TouchEventConverterEvdev::TouchEventConverterEvdev(
    base::ScopedFD fd,
    base::FilePath path,
    int id,
    const EventDeviceInfo& devinfo,
    SharedPalmDetectionFilterState* shared_palm_state,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      input_device_fd_(std::move(fd)),
      dispatcher_(dispatcher),
      palm_detection_filter_(
          CreatePalmDetectionFilter(devinfo, shared_palm_state)),
      palm_on_touch_major_max_(
          base::FeatureList::IsEnabled(kEnablePalmOnMaxTouchMajor)),
      palm_on_tool_type_palm_(
          base::FeatureList::IsEnabled(kEnablePalmOnToolTypePalm)),
      shared_palm_state_(shared_palm_state) {
  if (base::FeatureList::IsEnabled(kEnableNeuralPalmDetectionFilter) &&
      NeuralStylusPalmDetectionFilter::
          CompatibleWithNeuralStylusPalmDetectionFilter(devinfo)) {
    // When a neural net palm detector is enabled, we do not look at tool_type
    // nor the max size of the touch as indicators of palm, merely the NN
    // system.
    palm_on_tool_type_palm_ = palm_on_touch_major_max_ = false;
  }
  touch_evdev_debug_buffer_.Initialize(devinfo);
}

TouchEventConverterEvdev::~TouchEventConverterEvdev() = default;

// static
std::unique_ptr<TouchEventConverterEvdev> TouchEventConverterEvdev::Create(
    base::ScopedFD fd,
    base::FilePath path,
    int id,
    const EventDeviceInfo& devinfo,
    SharedPalmDetectionFilterState* shared_palm_state,
    DeviceEventDispatcherEvdev* dispatcher) {
  auto converter = std::make_unique<TouchEventConverterEvdev>(
      std::move(fd), std::move(path), id, devinfo, shared_palm_state,
      dispatcher);
  converter->Initialize(devinfo);
  return converter;
}

void TouchEventConverterEvdev::Initialize(const EventDeviceInfo& info) {
  has_mt_ = info.HasMultitouch();
  has_pen_ = info.HasKeyEvent(BTN_TOOL_PEN);
  int32_t touch_major_res =
      info.GetAbsInfoByCode(ABS_MT_TOUCH_MAJOR).resolution;
  int32_t touch_minor_res =
      info.GetAbsInfoByCode(ABS_MT_TOUCH_MINOR).resolution;

  if (has_mt_) {
    pressure_min_ = info.GetAbsMinimum(ABS_MT_PRESSURE);
    pressure_max_ = info.GetAbsMaximum(ABS_MT_PRESSURE);
    x_min_tuxels_ = info.GetAbsMinimum(ABS_MT_POSITION_X);
    x_num_tuxels_ = info.GetAbsMaximum(ABS_MT_POSITION_X) - x_min_tuxels_ + 1;
    x_res_ = info.GetAbsInfoByCode(ABS_MT_POSITION_X).resolution;
    y_min_tuxels_ = info.GetAbsMinimum(ABS_MT_POSITION_Y);
    y_num_tuxels_ = info.GetAbsMaximum(ABS_MT_POSITION_Y) - y_min_tuxels_ + 1;
    y_res_ = info.GetAbsInfoByCode(ABS_MT_POSITION_Y).resolution;
    tool_x_min_tuxels_ = info.GetAbsMinimum(ABS_MT_TOOL_X);
    tool_x_num_tuxels_ =
        info.GetAbsMaximum(ABS_MT_TOOL_X) - tool_x_min_tuxels_ + 1;
    tool_x_res_ = info.GetAbsInfoByCode(ABS_MT_TOOL_X).resolution;
    tool_y_min_tuxels_ = info.GetAbsMinimum(ABS_MT_TOOL_Y);
    tool_y_num_tuxels_ =
        info.GetAbsMaximum(ABS_MT_TOOL_Y) - tool_y_min_tuxels_ + 1;
    tool_y_res_ = info.GetAbsInfoByCode(ABS_MT_TOOL_Y).resolution;
    touch_points_ =
        std::min<int>(info.GetAbsMaximum(ABS_MT_SLOT) + 1, kNumTouchEvdevSlots);
    major_max_ = info.GetAbsMaximum(ABS_MT_TOUCH_MAJOR);
    current_slot_ = info.GetAbsValue(ABS_MT_SLOT);
    orientation_min_ = info.GetAbsMinimum(ABS_MT_ORIENTATION);
    orientation_max_ = info.GetAbsMaximum(ABS_MT_ORIENTATION);
  } else {
    pressure_min_ = info.GetAbsMinimum(ABS_PRESSURE);
    pressure_max_ = info.GetAbsMaximum(ABS_PRESSURE);
    x_min_tuxels_ = info.GetAbsMinimum(ABS_X);
    x_num_tuxels_ = info.GetAbsMaximum(ABS_X) - x_min_tuxels_ + 1;
    x_res_ = info.GetAbsInfoByCode(ABS_X).resolution;
    y_min_tuxels_ = info.GetAbsMinimum(ABS_Y);
    y_num_tuxels_ = info.GetAbsMaximum(ABS_Y) - y_min_tuxels_ + 1;
    y_res_ = info.GetAbsInfoByCode(ABS_Y).resolution;
    tool_x_min_tuxels_ = x_min_tuxels_;
    tool_x_num_tuxels_ = x_num_tuxels_;
    tool_x_res_ = x_res_;
    tool_y_min_tuxels_ = y_min_tuxels_;
    tool_y_num_tuxels_ = y_num_tuxels_;
    tool_y_res_ = y_res_;
    tilt_x_min_ = info.GetAbsMinimum(ABS_TILT_X);
    tilt_y_min_ = info.GetAbsMinimum(ABS_TILT_Y);
    tilt_x_range_ = info.GetAbsMaximum(ABS_TILT_X) - tilt_x_min_;
    tilt_y_range_ = info.GetAbsMaximum(ABS_TILT_Y) - tilt_y_min_;

    // No orientation without mt.
    orientation_min_ = orientation_max_ = 0;
    touch_points_ = 1;
    major_max_ = 0;
    current_slot_ = 0;
  }
  if (info.HasAbsEvent(ABS_MT_TOOL_X)) {
    DCHECK_EQ(tool_x_res_, x_res_)
        << "Expected tool X resolution to be identical as position X.";
  }
  if (info.HasAbsEvent(ABS_MT_TOOL_Y)) {
    DCHECK_EQ(tool_y_res_, y_res_)
        << "Expected tool Y resolution to be identical as position Y.";
  }

  x_scale_ = GetFingerSizeScale(touch_major_res, x_res_) / 2.0f;
  y_scale_ = GetFingerSizeScale(touch_minor_res, y_res_) / 2.0f;
  rotated_x_scale_ = GetFingerSizeScale(touch_minor_res, x_res_) / 2.0f;
  rotated_y_scale_ = GetFingerSizeScale(touch_major_res, y_res_) / 2.0f;

  quirk_left_mouse_button_ =
      !has_mt_ && !info.HasKeyEvent(BTN_TOUCH) && info.HasKeyEvent(BTN_LEFT);

  // TODO(denniskempin): Use EVIOCGKEY to synchronize key state.

  events_.resize(touch_points_);
  held_events_.resize(touch_points_);
  bool cancelled_state = false;
  if (has_mt_) {
    for (size_t i = 0; i < events_.size(); ++i) {
      events_[i].x = info.GetAbsMtSlotValueWithDefault(ABS_MT_POSITION_X, i, 0);
      events_[i].y = info.GetAbsMtSlotValueWithDefault(ABS_MT_POSITION_Y, i, 0);
      events_[i].tracking_id = info.GetAbsMtSlotValueWithDefault(
          ABS_MT_TRACKING_ID, i, kTrackingIdForUnusedSlot);
      events_[i].touching = (events_[i].tracking_id >= 0);
      events_[i].slot = i;

      // Dirty the slot so we'll update the consumer at the first opportunity.
      events_[i].altered = true;

      // Optional bits.
      int touch_major =
          info.GetAbsMtSlotValueWithDefault(ABS_MT_TOUCH_MAJOR, i, 0);
      int touch_minor =
          info.GetAbsMtSlotValueWithDefault(ABS_MT_TOUCH_MINOR, i, 0);
      events_[i].orientation =
          info.GetAbsMtSlotValueWithDefault(ABS_MT_ORIENTATION, i, 0);
      events_[i].pressure = ScalePressure(
          info.GetAbsMtSlotValueWithDefault(ABS_MT_PRESSURE, i, 0));
      int tool_type = info.GetAbsMtSlotValueWithDefault(ABS_MT_TOOL_TYPE, i,
                                                        MT_TOOL_FINGER);
      events_[i].tool_type = tool_type;
      events_[i].major = touch_major;
      events_[i].minor = touch_minor;
      events_[i].stylus_button = false;
      if (events_[i].cancelled)
        cancelled_state = true;
    }
  } else {
    // TODO(spang): Add key state to EventDeviceInfo to allow initial contact.
    // (and make sure to take into account quirk_left_mouse_button_)
    events_[0].x = 0;
    events_[0].y = 0;
    events_[0].tracking_id = kTrackingIdForUnusedSlot;
    events_[0].touching = false;
    events_[0].slot = 0;
    events_[0].radius_x = 0;
    events_[0].radius_y = 0;
    events_[0].orientation = 0;
    events_[0].pressure = 0;
    events_[0].tool_code = 0;
    events_[0].tilt_x = 0;
    events_[0].tilt_y = 0;
    events_[0].cancelled = false;
  }
  if (cancelled_state)
    MaybeCancelAllTouches();

  false_touch_finder_ = FalseTouchFinder::Create(GetTouchscreenSize());

  if (base::FeatureList::IsEnabled(kEnableHeatmapPalmDetection)) {
    auto hidraw_model_id = GetHidrawModelId(info);
    if (hidraw_model_id != HeatmapPalmDetector::ModelId::kNotSupported) {
      support_heatmap_palm_detection_ = true;
      auto hidraw_device_dir = base::FilePath("/sys/class/input")
                                   .Append(path_.BaseName())
                                   .Append("device/device/hidraw");
      auto hidraw_device_path = base::FilePath("/dev").Append(
          GetHidrawPath(hidraw_device_dir).BaseName());
      auto* palm_detector = HeatmapPalmDetector::GetInstance();
      if (palm_detector) {
        palm_detector->Start(hidraw_model_id, hidraw_device_path.value(),
                             GetCropHeatmap(info));
      }
    }
  }
}

void TouchEventConverterEvdev::Reinitialize() {
  EventDeviceInfo info;
  if (!info.Initialize(fd_, path_)) {
    LOG(ERROR) << "Failed to synchronize state for touch device: "
               << path_.value();
    Stop();
    return;
  }
  Initialize(info);
}

bool TouchEventConverterEvdev::HasTouchscreen() const {
  return true;
}

bool TouchEventConverterEvdev::HasPen() const {
  return has_pen_;
}

gfx::Size TouchEventConverterEvdev::GetTouchscreenSize() const {
  return gfx::Size(x_num_tuxels_, y_num_tuxels_);
}

int TouchEventConverterEvdev::GetTouchPoints() const {
  return touch_points_;
}

void TouchEventConverterEvdev::OnEnabled() {}

void TouchEventConverterEvdev::OnDisabled() {
  ReleaseTouches();
  if (enable_palm_suppression_callback_) {
    enable_palm_suppression_callback_.Run(false);
  }
}

void TouchEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev",
               "TouchEventConverterEvdev::OnFileCanReadWithoutBlocking", "fd",
               fd);

  input_event inputs[kNumTouchEvdevSlots * 6 + 1];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  for (unsigned i = 0; i < read_size / sizeof(*inputs); i++) {
    if (!has_mt_) {
      // Emulate the device as an MT device with only 1 slot by inserting extra
      // MT protocol events in the stream.
      EmulateMultitouchEvent(inputs[i]);
    }

    ProcessMultitouchEvent(inputs[i]);
  }
}

void TouchEventConverterEvdev::DumpTouchEventLog(const char* filename) {
  touch_evdev_debug_buffer_.DumpLog(filename);
}

void TouchEventConverterEvdev::SetTouchEventLoggingEnabled(bool enabled) {
  touch_logging_enabled_ = enabled;
}

void TouchEventConverterEvdev::SetPalmSuppressionCallback(
    const base::RepeatingCallback<void(bool)>& callback) {
  enable_palm_suppression_callback_ = callback;
}

void TouchEventConverterEvdev::SetReportStylusStateCallback(
    const ReportStylusStateCallback& callback) {
  report_stylus_state_callback_ = callback;
}

void TouchEventConverterEvdev::SetGetLatestStylusStateCallback(
    const GetLatestStylusStateCallback& callback) {
  get_latest_stylus_state_callback_ = callback;
}

void TouchEventConverterEvdev::ProcessMultitouchEvent(
    const input_event& input) {
  if (touch_logging_enabled_ && !has_pen_)
    touch_evdev_debug_buffer_.ProcessEvent(current_slot_, &input);

  if (input.type == EV_SYN) {
    ProcessSyn(input);
  } else if (dropped_events_) {
    // Do nothing. This branch indicates we have lost sync with the driver.
  } else if (input.type == EV_ABS) {
    if (events_.size() <= current_slot_) {
      LOG(ERROR) << "current_slot_ (" << current_slot_
                 << ") >= events_.size() (" << events_.size() << ")";
    } else {
      ProcessAbs(input);
    }
  } else if (input.type == EV_KEY) {
    ProcessKey(input);
  } else if (input.type == EV_MSC) {
    // Ignored.
  } else {
    NOTIMPLEMENTED() << "invalid type: " << input.type;
  }
}

void TouchEventConverterEvdev::EmulateMultitouchEvent(
    const input_event& event) {
  input_event emulated_event = event;

  if (event.type == EV_ABS) {
    emulated_event.code = AbsCodeToMtCode(event.code);
    if (emulated_event.code >= 0)
      ProcessMultitouchEvent(emulated_event);
  } else if (event.type == EV_KEY) {
    if (event.code == BTN_TOUCH || event.code == BTN_0 ||
        (quirk_left_mouse_button_ && event.code == BTN_LEFT)) {
      emulated_event.type = EV_ABS;
      emulated_event.code = ABS_MT_TRACKING_ID;
      emulated_event.value =
          event.value ? NextTrackingId() : kTrackingIdForUnusedSlot;
      ProcessMultitouchEvent(emulated_event);
    }
  }
}

void TouchEventConverterEvdev::ProcessKey(const input_event& input) {
  switch (input.code) {
    case BTN_STYLUS:
      events_[current_slot_].stylus_button = input.value;
      events_[current_slot_].altered = true;
      break;
    case BTN_TOOL_PEN:
    case BTN_TOOL_RUBBER:
      if (input.value > 0) {
        if (events_[current_slot_].tool_code != 0)
          break;
        events_[current_slot_].tool_code = input.code;
      } else {
        if (events_[current_slot_].tool_code != input.code)
          break;
        events_[current_slot_].tool_code = 0;
      }
      events_[current_slot_].altered = true;
      break;
    case BTN_LEFT:
    case BTN_0:
    case BTN_STYLUS2:
    case BTN_TOUCH:
      break;
    default:
      NOTIMPLEMENTED() << "invalid code for EV_KEY: " << input.code;
  }
}

void TouchEventConverterEvdev::ProcessAbs(const input_event& input) {
  switch (input.code) {
    case ABS_MT_TOUCH_MAJOR:
      events_[current_slot_].major = input.value;
      break;
    case ABS_MT_TOUCH_MINOR:
      events_[current_slot_].minor = input.value;
      break;
    case ABS_MT_ORIENTATION:
      events_[current_slot_].orientation = input.value;
      break;
    case ABS_MT_POSITION_X:
      events_[current_slot_].x = input.value;
      break;
    case ABS_MT_POSITION_Y:
      events_[current_slot_].y = input.value;
      break;
    case ABS_MT_TOOL_X:
      events_[current_slot_].tool_x = input.value;
      break;
    case ABS_MT_TOOL_Y:
      events_[current_slot_].tool_y = input.value;
      break;
    case ABS_MT_TOOL_TYPE:
      events_[current_slot_].tool_type = input.value;
      break;
    case ABS_MT_TRACKING_ID:
      UpdateTrackingId(current_slot_, input.value);
      break;
    case ABS_MT_PRESSURE:
      events_[current_slot_].pressure = ScalePressure(input.value);
      break;
    case ABS_MT_SLOT:
      if (input.value >= 0 &&
          static_cast<size_t>(input.value) < events_.size()) {
        current_slot_ = input.value;
      } else {
        LOG(ERROR) << "invalid touch event index: " << input.value;
        return;
      }
      break;
    case ABS_TILT_X:
      if (!has_mt_) {
        events_[0].tilt_x = ScaleTilt(input.value, tilt_x_min_, tilt_x_range_);
      }
      break;
    case ABS_TILT_Y:
      if (!has_mt_) {
        events_[0].tilt_y = ScaleTilt(input.value, tilt_y_min_, tilt_y_range_);
      }
      break;
    default:
      DVLOG(5) << "unhandled code for EV_ABS: " << input.code;
      return;
  }
  events_[current_slot_].altered = true;
}

void TouchEventConverterEvdev::ProcessSyn(const input_event& input) {
  switch (input.code) {
    case SYN_REPORT:
      ReportEvents(EventConverterEvdev::TimeTicksFromInputEvent(input));
      break;
    case SYN_DROPPED:
      // Some buffer has overrun. We ignore all events up to and
      // including the next SYN_REPORT.
      dropped_events_ = true;
      break;
    default:
      NOTIMPLEMENTED() << "invalid code for EV_SYN: " << input.code;
  }
}

EventType TouchEventConverterEvdev::GetEventTypeForTouch(
    const InProgressTouchEvdev& touch) {
  bool touch_is_alive = touch.touching && !touch.delayed && !touch.cancelled;
  bool touch_was_alive =
      touch.was_touching && !touch.was_delayed && !touch.was_cancelled;

  // Delaying an already live touch is not possible.
  DCHECK(!touch_was_alive || !touch.delayed);

  if ((!touch_was_alive && !touch_is_alive) || touch.was_cancelled) {
    // Ignore this touch; it was never born or has already died.
    return EventType::kUnknown;
  }

  if (!touch_was_alive) {
    // This touch has just been born.
    return EventType::kTouchPressed;
  }

  if (!touch_is_alive) {
    // This touch was alive but is now dead.
    if (touch.cancelled)
      return EventType::kTouchCancelled;  // Cancelled by driver or noise
                                          // filter.
    return EventType::kTouchReleased;     // Finger lifted.
  }

  return EventType::kTouchMoved;
}

void TouchEventConverterEvdev::ReportTouchEvent(
    const InProgressTouchEvdev& event,
    EventType event_type,
    base::TimeTicks timestamp) {
  ui::PointerDetails details(event.reported_tool_type, /* pointer_id*/ 0,
                             event.radius_x, event.radius_y, event.pressure,
                             /* twist */ 0, event.tilt_x, event.tilt_y);
  int flags = event.stylus_button ? ui::EF_LEFT_MOUSE_BUTTON : 0;
  dispatcher_->DispatchTouchEvent(TouchEventParams(
      input_device_.id, event.slot, event_type, gfx::PointF(event.x, event.y),
      details, timestamp, flags));
}

bool TouchEventConverterEvdev::MaybeCancelAllTouches() {
  // TODO(denniskempin): Remove once upper layers properly handle single
  // cancelled touches.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCancelAllTouches) ||
      base::FeatureList::IsEnabled(kEnableSingleCancelTouch)) {
    return false;
  }
  for (size_t i = 0; i < events_.size(); i++) {
    InProgressTouchEvdev* event = &events_[i];
    if (event->was_touching || event->touching) {
      event->cancelled = true;
      event->altered = true;
    }
  }
  return true;
}

bool TouchEventConverterEvdev::IsPalm(const InProgressTouchEvdev& touch) {
  if (support_heatmap_palm_detection_) {
    auto* palm_detector = HeatmapPalmDetector::GetInstance();
    if (palm_detector && palm_detector->IsReady()) {
      return palm_detector->IsPalm(touch.tracking_id);
    }
  }

  if (palm_on_tool_type_palm_ && touch.tool_type == MT_TOOL_PALM)
    return true;
  else if (palm_on_touch_major_max_ && major_max_ > 0 &&
           touch.major == major_max_)
    return true;
  return false;
}

void TouchEventConverterEvdev::ReportEvents(base::TimeTicks timestamp) {
  if (dropped_events_) {
    Reinitialize();
    dropped_events_ = false;
  }

  if (support_heatmap_palm_detection_) {
    auto* palm_detector = HeatmapPalmDetector::GetInstance();
    if (palm_detector) {
      std::vector<int> tracking_ids;
      for (size_t i = 0; i < events_.size(); i++) {
        if (events_[i].tracking_id >= 0) {
          tracking_ids.push_back(events_[i].tracking_id);
        }
      }
      palm_detector->AddTouchRecord(base::Time::Now(), tracking_ids);
    }
  }

  if (false_touch_finder_)
    false_touch_finder_->HandleTouches(events_, timestamp);
  std::bitset<kNumTouchEvdevSlots> hold, suppress;
  {
    if (get_latest_stylus_state_callback_) {
      const InProgressStylusState* latest_stylus_state = nullptr;
      get_latest_stylus_state_callback_.Run(&latest_stylus_state);
      if (latest_stylus_state == nullptr) {
        VLOG(1) << "latest_stylus_state = nullptr";
      } else {
        // TODO(alanlxl):  do something with latest_stylus_state.
      }
    }
  }
  {
    SCOPED_UMA_HISTOGRAM_TIMER(kPalmFilterTimerEventName);
    palm_detection_filter_->Filter(events_, timestamp, &hold, &suppress);
  }
  for (size_t i = 0; i < events_.size(); i++) {
    InProgressTouchEvdev* event = &events_[i];
    if (IsPalm(*event)) {
      event->cancelled = true;
    }
    event->held |= hold.test(i);
    event->cancelled |= suppress.test(i);
    if (event->altered && event->cancelled) {
      if (MaybeCancelAllTouches()) {
        // If all touches were cancelled, break out of this loop.
        break;
      }
    }
  }

  UpdateSharedPalmState(timestamp);
  DetectRepeatedTouch(timestamp);
  RecordMetrics(timestamp);

  for (size_t i = 0; i < events_.size(); i++) {
    InProgressTouchEvdev* event = &events_[i];
    if (!event->altered)
      continue;
    if (false_touch_finder_)
      event->delayed = false_touch_finder_->SlotShouldDelay(event->slot);

    if (event->held) {
      // For held events, we update the event state appropriately, and then stop
      // processing.
      held_events_[i].push(std::make_pair(*event, timestamp));
      event->was_cancelled = event->cancelled;
      event->was_touching = event->touching;
      event->was_delayed = event->delayed;
      event->altered = false;
      event->held = false;
      // Do nothing here.
      continue;
    }
    if ((event->was_cancelled || event->cancelled) &&
        !held_events_[i].empty()) {
      // This event is cancelled, but we also have a queue of events "held" . We
      // need to clear the queue and update state appropriately.
      auto first_held_event = held_events_[i].front().first;
      event->was_touching = first_held_event.was_touching;
      event->was_cancelled = first_held_event.was_cancelled;
      event->was_delayed = first_held_event.was_delayed;

      // Quick delete everything in the queue.
      auto empty_q =
          std::queue<std::pair<InProgressTouchEvdev, base::TimeTicks>>();
      held_events_[i].swap(empty_q);
      UMA_HISTOGRAM_COUNTS_100(kHoldCountAtCancelEventName, empty_q.size());
    }

    if (!held_events_[i].empty()) {
      UMA_HISTOGRAM_COUNTS_100(kHoldCountAtReleaseEventName,
                               held_events_[i].size());
      while (!held_events_[i].empty()) {
        auto held_event = held_events_[i].front();
        held_events_[i].pop();
        held_event.first.held = false;
        held_event.first.was_held = true;
        ProcessTouchEvent(&held_event.first, held_event.second);
      }
    }
    ProcessTouchEvent(event, timestamp);
    event->was_cancelled = event->cancelled;
    event->was_touching = event->touching;
    event->was_delayed = event->delayed;
    event->was_held = event->held;
    event->altered = false;
  }
}

void TouchEventConverterEvdev::UpdateSharedPalmState(
    base::TimeTicks timestamp) {
  if (type() != InputDeviceType::INPUT_DEVICE_INTERNAL) {
    return;
  }
  if (has_pen_) {
    if (events_[0].touching) {
      shared_palm_state_->latest_stylus_touch_time = timestamp;
    } else if (events_[0].was_touching) {
      shared_palm_state_->need_to_record_after_stylus_metrics = true;
    }
    return;
  }

  for (const auto& event : events_) {
    if (event.touching) {
      shared_palm_state_->latest_finger_touch_time = timestamp;
      break;
    }
  }

  shared_palm_state_->has_palm = false;
  for (const auto& event : events_) {
    if (event.touching && event.cancelled) {
      shared_palm_state_->has_palm = true;
      break;
    }
  }
}

void TouchEventConverterEvdev::DetectRepeatedTouch(base::TimeTicks timestamp) {
  if (!(type() == InputDeviceType::INPUT_DEVICE_INTERNAL && IsEnabled()) ||
      has_pen_ || x_res_ <= 0 || y_res_ <= 0) {
    return;
  }

  for (auto& event : events_) {
    if (!event.touching || event.was_touching) {
      continue;
    }
    event.start_x = event.x;
    event.start_y = event.y;

    for (auto it = cancelled_touches_.begin();
         it != cancelled_touches_.end();) {
      auto& touch = *it;
      if ((timestamp - touch.cancel_timestamp).InSeconds() >
          kMaxRepeatedTouchGapInSeconds) {
        it = cancelled_touches_.erase(it);
        continue;
      }
      float dist_sqr = pow(float(event.x - touch.start_x) / x_res_, 2) +
                       pow(float(event.y - touch.start_y) / y_res_, 2);
      if (dist_sqr < kRepeatedTouchThresholdInSquareMillimeter) {
        UMA_HISTOGRAM_BOOLEAN(kRepeatedTouchCountEventName, true);
        cancelled_touches_.erase(it);
        break;
      }
      ++it;
    }
  }

  for (const auto& event : events_) {
    if (event.touching && event.cancelled &&
        (!event.was_cancelled || !event.was_touching)) {
      // It is newly canceled touch.
      cancelled_touches_.insert(
          CancelledTouch(timestamp, event.start_x, event.start_y));
    }
  }
}

void TouchEventConverterEvdev::RecordMetrics(base::TimeTicks timestamp) {
  // Only record metrics when the converter is enabled. While it is disabled,
  // stylus touch could generate fake event to cancel all finger touches, we
  // should ignore them for recording metrics.
  if (!(type() == InputDeviceType::INPUT_DEVICE_INTERNAL && IsEnabled())) {
    return;
  }

  bool touching = false;
  for (const auto& event : events_) {
    if (event.touching) {
      touching = true;
      break;
    }
  }
  if (!touching) {
    return;
  }

  if (!session_start_time_) {
    session_start_time_ = timestamp;
    last_touch_is_palm_ = false;
  }
  record_session_timer_.Start(
      FROM_HERE, base::Seconds(kMaxTouchSessionGapInSeconds),
      base::BindOnce(&TouchEventConverterEvdev::RecordSession,
                     weak_factory_.GetWeakPtr(),
                     timestamp - session_start_time_.value()));

  if (has_pen_) {
    if (events_[0].touching && !events_[0].was_touching) {
      base::TimeDelta gap =
          timestamp - shared_palm_state_->latest_finger_touch_time;
      UMA_HISTOGRAM_TIMES(kTouchGapBeforeStylusEventName, gap);
      UMA_HISTOGRAM_ENUMERATION(
          kTouchTypeBeforeStylusEventName,
          gap.InMilliseconds() >= kMaxTouchStylusGapInMs
              ? TouchType::kNone
              : (shared_palm_state_->has_palm ? TouchType::kPalm
                                              : TouchType::kFinger));

      if (shared_palm_state_->need_to_record_after_stylus_metrics) {
        shared_palm_state_->need_to_record_after_stylus_metrics = false;
        UMA_HISTOGRAM_TIMES(kTouchGapAfterStylusEventName,
                            base::Seconds(kMaxTouchStylusGapInSeconds));
        UMA_HISTOGRAM_ENUMERATION(kTouchTypeAfterStylusEventName,
                                  TouchType::kNone);
      }
    }
  } else {
    if (shared_palm_state_->has_palm && !last_touch_is_palm_) {
      UMA_HISTOGRAM_BOOLEAN(kPalmTouchCountEventName, true);
    }
    last_touch_is_palm_ = shared_palm_state_->has_palm;

    if (shared_palm_state_->need_to_record_after_stylus_metrics) {
      shared_palm_state_->need_to_record_after_stylus_metrics = false;
      base::TimeDelta gap =
          timestamp - shared_palm_state_->latest_stylus_touch_time;
      UMA_HISTOGRAM_TIMES(kTouchGapAfterStylusEventName, gap);
      UMA_HISTOGRAM_ENUMERATION(
          kTouchTypeAfterStylusEventName,
          shared_palm_state_->has_palm ? TouchType::kPalm : TouchType::kFinger);
    }
  }
}

void TouchEventConverterEvdev::RecordSession(base::TimeDelta session_length) {
  session_start_time_ = std::nullopt;
  if (session_length.InMilliseconds() <= 0) {
    return;
  }
  if (has_pen_) {
    UMA_HISTOGRAM_TIMES(kStylusSessionLengthEventName, session_length);
    UMA_HISTOGRAM_BOOLEAN(kStylusSessionCountEventName, true);
  } else {
    UMA_HISTOGRAM_TIMES(kTouchSessionLengthEventName, session_length);
    UMA_HISTOGRAM_BOOLEAN(kTouchSessionCountEventName, true);
  }
}

void TouchEventConverterEvdev::ProcessTouchEvent(InProgressTouchEvdev* event,
                                                 base::TimeTicks timestamp) {
  if (enable_palm_suppression_callback_) {
    enable_palm_suppression_callback_.Run(event->tool_code > 0);
  }

  if (report_stylus_state_callback_ && !event->cancelled) {
    report_stylus_state_callback_.Run(*event, x_res_, y_res_, timestamp);
  }

  EventType event_type = GetEventTypeForTouch(*event);

  // The tool type is fixed with the touch pressed event and does not change.
  if (event_type == EventType::kTouchPressed) {
    event->reported_tool_type = GetEventPointerType(event->tool_code);
  }
  if (event_type != EventType::kUnknown) {
    UpdateRadiusFromTouchWithOrientation(event);
    ReportTouchEvent(*event, event_type, timestamp);
  }
}

void TouchEventConverterEvdev::UpdateTrackingId(int slot, int tracking_id) {
  InProgressTouchEvdev* event = &events_[slot];

  if (event->tracking_id == tracking_id)
    return;

  if (support_heatmap_palm_detection_) {
    auto* palm_detector = HeatmapPalmDetector::GetInstance();
    if (palm_detector) {
      palm_detector->RemoveTouch(event->tracking_id);
    }
  }

  event->tracking_id = tracking_id;
  event->touching = (tracking_id >= 0);
  event->altered = true;

  if (tracking_id >= 0) {
    event->was_cancelled = false;
    event->cancelled = !IsEnabled();
  }
}

void TouchEventConverterEvdev::ReleaseTouches() {
  for (size_t slot = 0; slot < events_.size(); slot++) {
    events_[slot].stylus_button = false;
    events_[slot].cancelled = true;
    events_[slot].altered = true;
  }

  ReportEvents(EventTimeForNow());
}

float TouchEventConverterEvdev::ScalePressure(int32_t value) const {
  float pressure = value - pressure_min_;
  if (pressure_max_ - pressure_min_)
    pressure /= pressure_max_ - pressure_min_;
  if (pressure > 1.0)
    pressure = 1.0;
  return pressure;
}

bool TouchEventConverterEvdev::SupportsOrientation() const {
  // TODO(b/185318572): Support more complex orientation reports than the
  // simplified 0/1.
  return orientation_max_ == 1 && orientation_min_ == 0;
}

void TouchEventConverterEvdev::UpdateRadiusFromTouchWithOrientation(
    InProgressTouchEvdev* event) const {
  if (!SupportsOrientation() || event->orientation == 1) {
    event->radius_x = event->major * x_scale_;
    event->radius_y = event->minor * y_scale_;
  } else {
    event->radius_x = event->minor * rotated_x_scale_;
    event->radius_y = event->major * rotated_y_scale_;
  }
}

int TouchEventConverterEvdev::NextTrackingId() {
  return next_tracking_id_++ & kMaxTrackingId;
}

std::ostream& TouchEventConverterEvdev::DescribeForLog(std::ostream& os) const {
  os << "class=ui::TouchEventConverterEvdev id=" << input_device_.id
     << std::endl
     << " has_mt=" << has_mt_ << std::endl
     << " has_pen=" << has_pen_ << std::endl
     << " quirk_left_mouse_button=" << quirk_left_mouse_button_ << std::endl
     << " pressure_min=" << pressure_min_ << std::endl
     << " pressure_max=" << pressure_max_ << std::endl
     << " orientation_min=" << orientation_min_ << std::endl
     << " orientation_max=" << orientation_max_ << std::endl
     << " tilt_x_min=" << tilt_x_min_ << std::endl
     << " tilt_x_range=" << tilt_x_range_ << std::endl
     << " tilt_y_min=" << tilt_y_min_ << std::endl
     << " tilt_y_range=" << tilt_y_range_ << std::endl
     << " x_res=" << x_res_ << std::endl
     << " y_res=" << y_res_ << std::endl
     << " x_min_tuxels=" << x_min_tuxels_ << std::endl
     << " x_num_tuxels=" << x_num_tuxels_ << std::endl
     << " y_min_tuxels=" << y_min_tuxels_ << std::endl
     << " y_num_tuxels=" << y_num_tuxels_ << std::endl
     << " tool_x_res=" << tool_x_res_ << std::endl
     << " tool_y_res=" << tool_y_res_ << std::endl
     << " tool_x_min_tuxels=" << tool_x_min_tuxels_ << std::endl
     << " tool_x_num_tuxels=" << tool_x_num_tuxels_ << std::endl
     << " tool_y_min_tuxels=" << tool_y_min_tuxels_ << std::endl
     << " tool_y_num_tuxels=" << tool_y_num_tuxels_ << std::endl
     << " x_scale=" << x_scale_ << std::endl
     << " y_scale=" << y_scale_ << std::endl
     << " rotated_x_scale=" << rotated_x_scale_ << std::endl
     << " rotated_y_scale=" << rotated_y_scale_ << std::endl
     << " touch_points=" << touch_points_ << std::endl
     << " major_max=" << major_max_ << std::endl
     << " touch_logging_enabled=" << touch_logging_enabled_ << std::endl
     << " palm_on_touch_major_max=" << palm_on_touch_major_max_ << std::endl
     << " palm_on_tool_type_palm=" << palm_on_tool_type_palm_ << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

const char TouchEventConverterEvdev::kHoldCountAtReleaseEventName[] =
    "Ozone.TouchEventConverterEvdev.HoldCountAtRelease";
const char TouchEventConverterEvdev::kHoldCountAtCancelEventName[] =
    "Ozone.TouchEventConverterEvdev.HoldCountAtCancel";
const char TouchEventConverterEvdev::kPalmFilterTimerEventName[] =
    "Ozone.TouchEventConverterEvdev.PalmDetectionFilterTime";
const char TouchEventConverterEvdev::kPalmTouchCountEventName[] =
    "Ozone.TouchEventConverterEvdev.PalmTouchCount";
const char TouchEventConverterEvdev::kRepeatedTouchCountEventName[] =
    "Ozone.TouchEventConverterEvdev.RepeatedTouchCount";
const char TouchEventConverterEvdev::kTouchGapAfterStylusEventName[] =
    "Ozone.TouchEventConverterEvdev.TouchGapAfterStylus";
const char TouchEventConverterEvdev::kTouchGapBeforeStylusEventName[] =
    "Ozone.TouchEventConverterEvdev.TouchGapBeforeStylus";
const char TouchEventConverterEvdev::kTouchTypeAfterStylusEventName[] =
    "Ozone.TouchEventConverterEvdev.TouchTypeAfterStylus";
const char TouchEventConverterEvdev::kTouchTypeBeforeStylusEventName[] =
    "Ozone.TouchEventConverterEvdev.TouchTypeBeforeStylus";
const char TouchEventConverterEvdev::kTouchSessionCountEventName[] =
    "Ozone.TouchEventConverterEvdev.TouchSessionCount";
const char TouchEventConverterEvdev::kTouchSessionLengthEventName[] =
    "Ozone.TouchEventConverterEvdev.TouchSessionLength";
const char TouchEventConverterEvdev::kStylusSessionCountEventName[] =
    "Ozone.TouchEventConverterEvdev.StylusSessionCount";
const char TouchEventConverterEvdev::kStylusSessionLengthEventName[] =
    "Ozone.TouchEventConverterEvdev.StylusSessionLength";
}  // namespace ui
