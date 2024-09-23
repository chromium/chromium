// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/devices/x11/device_data_manager_x11.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/future.h"

// XIScrollClass was introduced in XI 2.1 so we need to define it here
// for backward-compatibility with older versions of XInput.
#if !defined(XIScrollClass)
#define XIScrollClass 3
#endif

// Copied from xserver-properties.h
#define AXIS_LABEL_PROP_REL_HWHEEL "Rel Horiz Wheel"
#define AXIS_LABEL_PROP_REL_WHEEL "Rel Vert Wheel"

// CMT specific timings
#define AXIS_LABEL_PROP_ABS_DBL_START_TIME "Abs Dbl Start Timestamp"
#define AXIS_LABEL_PROP_ABS_DBL_END_TIME "Abs Dbl End Timestamp"

// Ordinal values
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X "Abs Dbl Ordinal X"
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y "Abs Dbl Ordinal Y"

// Fling properties
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VX "Abs Dbl Fling X Velocity"
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VY "Abs Dbl Fling Y Velocity"
#define AXIS_LABEL_PROP_ABS_FLING_STATE "Abs Fling State"

#define AXIS_LABEL_PROP_ABS_FINGER_COUNT "Abs Finger Count"

// Cros metrics gesture from touchpad
#define AXIS_LABEL_PROP_ABS_METRICS_TYPE "Abs Metrics Type"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1 "Abs Dbl Metrics Data 1"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2 "Abs Dbl Metrics Data 2"

// Touchscreen multi-touch
#define AXIS_LABEL_ABS_MT_TOUCH_MAJOR "Abs MT Touch Major"
#define AXIS_LABEL_ABS_MT_TOUCH_MINOR "Abs MT Touch Minor"
#define AXIS_LABEL_ABS_MT_ORIENTATION "Abs MT Orientation"
#define AXIS_LABEL_ABS_MT_PRESSURE "Abs MT Pressure"
#define AXIS_LABEL_ABS_MT_POSITION_X "Abs MT Position X"
#define AXIS_LABEL_ABS_MT_POSITION_Y "Abs MT Position Y"
#define AXIS_LABEL_ABS_MT_TRACKING_ID "Abs MT Tracking ID"
#define AXIS_LABEL_TOUCH_TIMESTAMP "Touch Timestamp"

// Stylus with absolute x y pressure and tilt info
#define AXIS_LABEL_PROP_ABS_X "Abs X"
#define AXIS_LABEL_PROP_ABS_Y "Abs Y"
#define AXIS_LABEL_PROP_ABS_PRESSURE "Abs Pressure"
#define AXIS_LABEL_PROP_ABS_TILT_X "Abs Tilt X"
#define AXIS_LABEL_PROP_ABS_TILT_Y "Abs Tilt Y"

// When you add new data types, please make sure the order here is aligned
// with the order in the DataType enum in the header file because we assume
// they are in sync when updating the device list (see UpdateDeviceList).
constexpr const char* kCachedAtoms[] = {
    AXIS_LABEL_PROP_REL_HWHEEL,
    AXIS_LABEL_PROP_REL_WHEEL,
    AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X,
    AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y,
    AXIS_LABEL_PROP_ABS_DBL_START_TIME,
    AXIS_LABEL_PROP_ABS_DBL_END_TIME,
    AXIS_LABEL_PROP_ABS_DBL_FLING_VX,
    AXIS_LABEL_PROP_ABS_DBL_FLING_VY,
    AXIS_LABEL_PROP_ABS_FLING_STATE,
    AXIS_LABEL_PROP_ABS_METRICS_TYPE,
    AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1,
    AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2,
    AXIS_LABEL_PROP_ABS_FINGER_COUNT,
    AXIS_LABEL_ABS_MT_TOUCH_MAJOR,
    AXIS_LABEL_ABS_MT_TOUCH_MINOR,
    AXIS_LABEL_ABS_MT_ORIENTATION,
    AXIS_LABEL_ABS_MT_PRESSURE,
    AXIS_LABEL_ABS_MT_POSITION_X,
    AXIS_LABEL_ABS_MT_POSITION_Y,
    AXIS_LABEL_ABS_MT_TRACKING_ID,
    AXIS_LABEL_TOUCH_TIMESTAMP,
    AXIS_LABEL_PROP_ABS_X,
    AXIS_LABEL_PROP_ABS_Y,
    AXIS_LABEL_PROP_ABS_PRESSURE,
    AXIS_LABEL_PROP_ABS_TILT_X,
    AXIS_LABEL_PROP_ABS_TILT_Y,
};

// Make sure the sizes of enum and |kCachedAtoms| are aligned.
static_assert(std::size(kCachedAtoms) ==
                  ui::DeviceDataManagerX11::DT_LAST_ENTRY,
              "kCachedAtoms count / enum mismatch");

// Constants for checking if a data type lies in the range of CMT/Touch data
// types.
const int kCMTDataTypeStart = ui::DeviceDataManagerX11::DT_CMT_SCROLL_X;
const int kCMTDataTypeEnd = ui::DeviceDataManagerX11::DT_CMT_FINGER_COUNT;
const int kTouchDataTypeStart = ui::DeviceDataManagerX11::DT_TOUCH_MAJOR;
const int kTouchDataTypeEnd = ui::DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP;
const int kStylusDataTypeStart = ui::DeviceDataManagerX11::DT_STYLUS_POSITION_X;
const int kStylusDataTypeEnd = ui::DeviceDataManagerX11::DT_STYLUS_TILT_Y;

namespace ui {

namespace {

template <typename Iterator>
Iterator FindDeviceWithId(Iterator begin,
                          Iterator end,
                          x11::Input::DeviceId id) {
  for (auto it = begin; it != end; ++it) {
    if (static_cast<x11::Input::DeviceId>(it->id) == id)
      return it;
  }
  return end;
}

// Disables high precision scrolling in X11
const char kDisableHighPrecisionScrolling[] =
    "disable-high-precision-scrolling";

bool IsHighPrecisionScrollingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableHighPrecisionScrolling);
}

// Identical to double_to_fp3232 from xserver's inpututils.c
x11::Input::Fp3232 DoubleToFp3232(double in) {
  x11::Input::Fp3232 ret;

  double tmp = floor(in);
  int32_t integral = tmp;

  tmp = (in - integral) * (1ULL << 32);
  uint32_t frac_d = tmp;

  ret.integral = integral;
  ret.frac = frac_d;
  return ret;
}

// Identical to FP3232_TO_DOUBLE from libxi's XExtInt.c
double Fp3232ToDouble(const x11::Input::Fp3232& x) {
  return static_cast<double>(x.integral) +
         static_cast<double>(x.frac) / (1ULL << 32);
}

bool GetSourceId(const x11::Event& x11_event, uint16_t* sourceid) {
  x11::Input::DeviceId source{};
  if (auto* dev = x11_event.As<x11::Input::DeviceEvent>())
    source = dev->sourceid;
  else if (auto* dev_change = x11_event.As<x11::Input::DeviceChangedEvent>())
    source = dev_change->sourceid;
  else if (auto* crossing = x11_event.As<x11::Input::CrossingEvent>())
    source = crossing->sourceid;
  else
    return false;
  uint16_t source16 = static_cast<uint16_t>(source);
  if (source16 >= DeviceDataManagerX11::kMaxDeviceNum)
    return false;
  *sourceid = source16;
  return true;
}

}  // namespace

bool DeviceDataManagerX11::IsCMTDataType(const int type) {
  return (type >= kCMTDataTypeStart) && (type <= kCMTDataTypeEnd);
}

bool DeviceDataManagerX11::IsTouchDataType(const int type) {
  return (type >= kTouchDataTypeStart) && (type <= kTouchDataTypeEnd);
}

bool DeviceDataManagerX11::IsStylusDataType(const int type) {
  return (type >= kStylusDataTypeStart) && (type <= kStylusDataTypeEnd);
}

// static
void DeviceDataManagerX11::CreateInstance() {
  if (HasInstance())
    return;

  new DeviceDataManagerX11();

  // TODO(bruthig): Replace the DeleteInstance callbacks with explicit calls.
  base::AtExitManager::RegisterTask(
      base::BindOnce(DeviceDataManager::DeleteInstance));
}

// static
DeviceDataManagerX11* DeviceDataManagerX11::GetInstance() {
  return static_cast<DeviceDataManagerX11*>(DeviceDataManager::GetInstance());
}

DeviceDataManagerX11::DeviceDataManagerX11()
    : high_precision_scrolling_disabled_(IsHighPrecisionScrollingDisabled()) {
  CHECK(x11::Connection::Get());

  UpdateDeviceList(x11::Connection::Get());
  UpdateButtonMap();
}

DeviceDataManagerX11::~DeviceDataManagerX11() = default;

bool DeviceDataManagerX11::IsXInput2Available() const {
  return x11::Connection::Get()->xinput_version() >=
         std::pair<uint32_t, uint32_t>{2, 2};
}

void DeviceDataManagerX11::UpdateDeviceList(x11::Connection* connection) {
  cmt_devices_.reset();
  touchpads_.reset();
  stylus_.reset();
  master_pointers_.clear();
  for (int i = 0; i < kMaxDeviceNum; ++i) {
    valuator_count_[i] = 0;
    valuator_lookup_[i].clear();
    data_type_lookup_[i].clear();
    scroll_data_[i].horizontal.number = -1;
    scroll_data_[i].horizontal.seen = false;
    scroll_data_[i].vertical.number = -1;
    scroll_data_[i].vertical.seen = false;
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[i][j].clear();
  }

  // Find all the touchpad devices.
  const XDeviceList& dev_list =
      ui::DeviceListCacheX11::GetInstance()->GetXDeviceList(connection);
  x11::Atom xi_touchpad = x11::GetAtom("TOUCHPAD");
  for (const auto& device : dev_list) {
    if (device.device_type == xi_touchpad)
      touchpads_[device.device_id] = true;
  }

  if (!IsXInput2Available())
    return;

  // Update the structs with new valuator information
  const XIDeviceList& info_list =
      ui::DeviceListCacheX11::GetInstance()->GetXI2DeviceList(connection);
  x11::Atom atoms[DT_LAST_ENTRY];
  for (int data_type = 0; data_type < DT_LAST_ENTRY; ++data_type)
    atoms[data_type] = x11::GetAtom(kCachedAtoms[data_type]);

  for (const auto& info : info_list) {
    if (info.type == x11::Input::DeviceType::MasterPointer)
      master_pointers_.push_back(info.deviceid);

    // We currently handle only slave, non-keyboard devices
    if (info.type != x11::Input::DeviceType::SlavePointer &&
        info.type != x11::Input::DeviceType::FloatingSlave) {
      continue;
    }

    bool possible_stylus = false;
    bool possible_cmt = false;
    bool not_cmt = false;
    const auto deviceid = static_cast<uint16_t>(info.deviceid);

    for (const auto& device_class : info.classes) {
      if (device_class.valuator.has_value())
        ++valuator_count_[deviceid];
      else if (device_class.scroll.has_value())
        not_cmt = true;
    }

    // Skip devices that don't use any valuator
    if (!valuator_count_[deviceid])
      continue;

    valuator_lookup_[deviceid].resize(DT_LAST_ENTRY);
    data_type_lookup_[deviceid].resize(valuator_count_[deviceid],
                                       DT_LAST_ENTRY);
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[deviceid][j].resize(DT_LAST_ENTRY, 0);
    for (const auto& device_class : info.classes) {
      if (device_class.valuator.has_value()) {
        auto dt =
            UpdateValuatorClassDevice(*device_class.valuator, atoms, deviceid);
        if (IsStylusDataType(dt))
          possible_stylus = true;
        if (IsCMTDataType(dt))
          possible_cmt = true;
      } else if (device_class.scroll.has_value()) {
        UpdateScrollClassDevice(*device_class.scroll, deviceid);
      }
    }

    if (possible_stylus)
      stylus_[deviceid] = true;

    if (possible_cmt && !not_cmt)
      cmt_devices_[deviceid] = true;
  }
}

bool DeviceDataManagerX11::GetSlotNumber(const x11::Input::DeviceEvent& xiev,
                                         int* slot) {
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
  if (!factory->IsMultiTouchDevice(xiev.sourceid)) {
    *slot = 0;
    return true;
  }
  return factory->QuerySlotForTrackingID(xiev.detail, slot);
}

void DeviceDataManagerX11::GetEventRawData(const x11::Event& x11_event,
                                           EventData* data) {
  auto* xiev = x11_event.As<x11::Input::DeviceEvent>();
  if (!xiev)
    return;

  auto sourceid = static_cast<uint16_t>(xiev->sourceid);
  auto deviceid = static_cast<uint16_t>(xiev->deviceid);
  if (sourceid >= kMaxDeviceNum || deviceid >= kMaxDeviceNum)
    return;
  data->clear();
  const x11::Input::Fp3232* valuators = xiev->axisvalues.data();
  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (IsXinputMaskSet(xiev->valuator_mask.data(), i)) {
      int type = data_type_lookup_[sourceid][i];
      if (type != DT_LAST_ENTRY) {
        double valuator = Fp3232ToDouble(*valuators);
        (*data)[type] = valuator;
        if (IsTouchDataType(type)) {
          int slot = -1;
          if (GetSlotNumber(*xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
            last_seen_valuator_[sourceid][slot][type] = valuator;
        }
      }
      valuators++;
    }
  }
}

bool DeviceDataManagerX11::GetEventData(const x11::Event& x11_event,
                                        const DataType type,
                                        double* value) {
  auto* xiev = x11_event.As<x11::Input::DeviceEvent>();
  if (!xiev)
    return false;

  auto sourceid = static_cast<uint16_t>(xiev->sourceid);
  auto deviceid = static_cast<uint16_t>(xiev->deviceid);
  if (sourceid >= kMaxDeviceNum || deviceid >= kMaxDeviceNum)
    return false;
  if (valuator_lookup_[sourceid].empty())
    return false;

  if (type == DT_TOUCH_TRACKING_ID) {
    // With XInput2 MT, Tracking ID is provided in the detail field for touch
    // events.
    if (xiev->opcode == x11::Input::DeviceEvent::TouchBegin ||
        xiev->opcode == x11::Input::DeviceEvent::TouchEnd ||
        xiev->opcode == x11::Input::DeviceEvent::TouchUpdate) {
      *value = xiev->detail;
    } else {
      *value = 0;
    }
    return true;
  }

  int val_index = valuator_lookup_[sourceid][type].number;
  int slot = 0;
  if (val_index >= 0) {
    if (IsXinputMaskSet(xiev->valuator_mask.data(), val_index)) {
      const x11::Input::Fp3232* valuators = xiev->axisvalues.data();
      while (val_index--) {
        if (IsXinputMaskSet(xiev->valuator_mask.data(), val_index))
          ++valuators;
      }
      *value = Fp3232ToDouble(*valuators);
      if (IsTouchDataType(type)) {
        if (GetSlotNumber(*xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
          last_seen_valuator_[sourceid][slot][type] = *value;
      }
      return true;
    } else if (IsTouchDataType(type)) {
      if (GetSlotNumber(*xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
        *value = last_seen_valuator_[sourceid][slot][type];
    }
  }

  return false;
}

bool DeviceDataManagerX11::IsXIDeviceEvent(const x11::Event& x11_event) const {
  return x11_event.As<x11::Input::DeviceEvent>();
}

bool DeviceDataManagerX11::IsStylusXInputEvent(
    const x11::Event& x11_event) const {
  uint16_t source;
  if (!GetSourceId(x11_event, &source))
    return false;
  return stylus_[source];
}

bool DeviceDataManagerX11::IsTouchpadXInputEvent(
    const x11::Event& x11_event) const {
  uint16_t source;
  if (!GetSourceId(x11_event, &source))
    return false;
  return touchpads_[source];
}

bool DeviceDataManagerX11::IsCMTDeviceEvent(const x11::Event& x11_event) const {
  uint16_t source;
  if (!GetSourceId(x11_event, &source))
    return false;
  return cmt_devices_[source];
}

int DeviceDataManagerX11::GetScrollClassEventDetail(
    const x11::Event& x11_event) const {
  auto* xievent = x11_event.As<x11::Input::DeviceEvent>();
  if (!xievent)
    return SCROLL_TYPE_NO_SCROLL;
  auto sourceid = static_cast<uint16_t>(xievent->sourceid);
  if (sourceid >= kMaxDeviceNum)
    return SCROLL_TYPE_NO_SCROLL;
  int horizontal_id = scroll_data_[sourceid].horizontal.number;
  int vertical_id = scroll_data_[sourceid].vertical.number;
  return (horizontal_id != -1 &&
                  IsXinputMaskSet(xievent->valuator_mask.data(), horizontal_id)
              ? SCROLL_TYPE_HORIZONTAL
              : 0) |
         (vertical_id != -1 &&
                  IsXinputMaskSet(xievent->valuator_mask.data(), vertical_id)
              ? SCROLL_TYPE_VERTICAL
              : 0);
}

int DeviceDataManagerX11::GetScrollClassDeviceDetail(
    const x11::Event& x11_event) const {
  auto* xiev = x11_event.As<x11::Input::DeviceEvent>();
  if (!xiev)
    return SCROLL_TYPE_NO_SCROLL;

  auto sourceid = static_cast<uint16_t>(xiev->sourceid);
  auto deviceid = static_cast<uint16_t>(xiev->deviceid);
  if (sourceid >= kMaxDeviceNum || deviceid >= kMaxDeviceNum)
    return SCROLL_TYPE_NO_SCROLL;
  const ScrollInfo& device_data = scroll_data_[sourceid];
  return (device_data.vertical.number >= 0 ? SCROLL_TYPE_VERTICAL : 0) |
         (device_data.horizontal.number >= 0 ? SCROLL_TYPE_HORIZONTAL : 0);
}

bool DeviceDataManagerX11::IsCMTGestureEvent(const x11::Event& xev) const {
  return (IsScrollEvent(xev) || IsFlingEvent(xev) || IsCMTMetricsEvent(xev));
}

bool DeviceDataManagerX11::HasEventData(const x11::Event& xev,
                                        const DataType type) const {
  auto* xiev = xev.As<x11::Input::DeviceEvent>();
  if (!xiev)
    return false;
  auto sourceid = static_cast<uint16_t>(xiev->sourceid);
  if (sourceid >= kMaxDeviceNum)
    return false;
  if (type >= valuator_lookup_[sourceid].size())
    return false;
  const int idx = valuator_lookup_[sourceid][type].number;
  return (idx >= 0) && IsXinputMaskSet(xiev->valuator_mask.data(), idx);
}

bool DeviceDataManagerX11::IsScrollEvent(const x11::Event& x11_event) const {
  if (!IsCMTDeviceEvent(x11_event))
    return false;

  return (HasEventData(x11_event, DT_CMT_SCROLL_X) ||
          HasEventData(x11_event, DT_CMT_SCROLL_Y));
}

bool DeviceDataManagerX11::IsFlingEvent(const x11::Event& x11_event) const {
  if (!IsCMTDeviceEvent(x11_event))
    return false;

  return (HasEventData(x11_event, DT_CMT_FLING_X) &&
          HasEventData(x11_event, DT_CMT_FLING_Y) &&
          HasEventData(x11_event, DT_CMT_FLING_STATE));
}

bool DeviceDataManagerX11::IsCMTMetricsEvent(
    const x11::Event& x11_event) const {
  if (!IsCMTDeviceEvent(x11_event))
    return false;

  return (HasEventData(x11_event, DT_CMT_METRICS_TYPE) &&
          HasEventData(x11_event, DT_CMT_METRICS_DATA1) &&
          HasEventData(x11_event, DT_CMT_METRICS_DATA2));
}

bool DeviceDataManagerX11::HasGestureTimes(const x11::Event& x11_event) const {
  if (!IsCMTDeviceEvent(x11_event))
    return false;

  return (HasEventData(x11_event, DT_CMT_START_TIME) &&
          HasEventData(x11_event, DT_CMT_END_TIME));
}

void DeviceDataManagerX11::GetScrollOffsets(const x11::Event& xev,
                                            float* x_offset,
                                            float* y_offset,
                                            float* x_offset_ordinal,
                                            float* y_offset_ordinal,
                                            int* finger_count) {
  *x_offset = 0;
  *y_offset = 0;
  *x_offset_ordinal = 0;
  *y_offset_ordinal = 0;
  *finger_count = 2;

  EventData data;
  GetEventRawData(xev, &data);

  if (data.find(DT_CMT_SCROLL_X) != data.end())
    *x_offset = data[DT_CMT_SCROLL_X];
  if (data.find(DT_CMT_SCROLL_Y) != data.end())
    *y_offset = data[DT_CMT_SCROLL_Y];
  if (data.find(DT_CMT_ORDINAL_X) != data.end())
    *x_offset_ordinal = data[DT_CMT_ORDINAL_X];
  if (data.find(DT_CMT_ORDINAL_Y) != data.end())
    *y_offset_ordinal = data[DT_CMT_ORDINAL_Y];
  if (data.find(DT_CMT_FINGER_COUNT) != data.end())
    *finger_count = static_cast<int>(data[DT_CMT_FINGER_COUNT]);
}

void DeviceDataManagerX11::GetScrollClassOffsets(const x11::Event& x11_event,
                                                 double* x_offset,
                                                 double* y_offset) {
  DCHECK_NE(SCROLL_TYPE_NO_SCROLL, GetScrollClassDeviceDetail(x11_event));

  *x_offset = 0;
  *y_offset = 0;

  auto* xiev = x11_event.As<x11::Input::DeviceEvent>();
  if (!xiev)
    return;

  auto sourceid = static_cast<uint16_t>(xiev->sourceid);
  auto deviceid = static_cast<uint16_t>(xiev->deviceid);
  if (sourceid >= kMaxDeviceNum || deviceid >= kMaxDeviceNum)
    return;
  const x11::Input::Fp3232* valuators = xiev->axisvalues.data();

  ScrollInfo* info = &scroll_data_[sourceid];

  const int horizontal_number = info->horizontal.number;
  const int vertical_number = info->vertical.number;

  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (!IsXinputMaskSet(xiev->valuator_mask.data(), i))
      continue;
    auto valuator = Fp3232ToDouble(*valuators);
    if (i == horizontal_number)
      *x_offset = ExtractAndUpdateScrollOffset(&info->horizontal, valuator);
    else if (i == vertical_number)
      *y_offset = ExtractAndUpdateScrollOffset(&info->vertical, valuator);
    valuators++;
  }
}

void DeviceDataManagerX11::InvalidateScrollClasses(
    x11::Input::DeviceId device_id) {
  if (device_id == kAllDevices) {
    for (auto& i : scroll_data_) {
      i.horizontal.seen = false;
      i.vertical.seen = false;
    }
  } else {
    auto device16 = static_cast<uint16_t>(device_id);
    CHECK_LT(device16, kMaxDeviceNum);
    scroll_data_[device16].horizontal.seen = false;
    scroll_data_[device16].vertical.seen = false;
  }
}

void DeviceDataManagerX11::GetFlingData(const x11::Event& xev,
                                        float* vx,
                                        float* vy,
                                        float* vx_ordinal,
                                        float* vy_ordinal,
                                        bool* is_cancel) {
  *vx = 0;
  *vy = 0;
  *vx_ordinal = 0;
  *vy_ordinal = 0;
  *is_cancel = false;

  EventData data;
  GetEventRawData(xev, &data);

  if (data.find(DT_CMT_FLING_X) != data.end())
    *vx = data[DT_CMT_FLING_X];
  if (data.find(DT_CMT_FLING_Y) != data.end())
    *vy = data[DT_CMT_FLING_Y];
  if (data.find(DT_CMT_FLING_STATE) != data.end())
    *is_cancel = !!static_cast<unsigned int>(data[DT_CMT_FLING_STATE]);
  if (data.find(DT_CMT_ORDINAL_X) != data.end())
    *vx_ordinal = data[DT_CMT_ORDINAL_X];
  if (data.find(DT_CMT_ORDINAL_Y) != data.end())
    *vy_ordinal = data[DT_CMT_ORDINAL_Y];
}

void DeviceDataManagerX11::GetMetricsData(const x11::Event& xev,
                                          GestureMetricsType* type,
                                          float* data1,
                                          float* data2) {
  *type = kGestureMetricsTypeUnknown;
  *data1 = 0;
  *data2 = 0;

  EventData data;
  GetEventRawData(xev, &data);

  if (data.find(DT_CMT_METRICS_TYPE) != data.end()) {
    int val = static_cast<int>(data[DT_CMT_METRICS_TYPE]);
    if (val == 0)
      *type = kGestureMetricsTypeNoisyGround;
    else
      *type = kGestureMetricsTypeUnknown;
  }
  if (data.find(DT_CMT_METRICS_DATA1) != data.end())
    *data1 = data[DT_CMT_METRICS_DATA1];
  if (data.find(DT_CMT_METRICS_DATA2) != data.end())
    *data2 = data[DT_CMT_METRICS_DATA2];
}

int DeviceDataManagerX11::GetMappedButton(int button) {
  return button > 0 && static_cast<unsigned int>(button) <= button_map_.size()
             ? button_map_[button - 1]
             : button;
}

void DeviceDataManagerX11::UpdateButtonMap() {
  if (auto reply = x11::Connection::Get()->GetPointerMapping().Sync())
    button_map_ = std::move(reply->map);
}

void DeviceDataManagerX11::GetGestureTimes(const x11::Event& xev,
                                           double* start_time,
                                           double* end_time) {
  *start_time = 0;
  *end_time = 0;

  EventData data;
  GetEventRawData(xev, &data);

  if (data.find(DT_CMT_START_TIME) != data.end())
    *start_time = data[DT_CMT_START_TIME];
  if (data.find(DT_CMT_END_TIME) != data.end())
    *end_time = data[DT_CMT_END_TIME];
}

bool DeviceDataManagerX11::NormalizeData(x11::Input::DeviceId deviceid,
                                         const DataType type,
                                         double* value) {
  double max_value;
  double min_value;
  if (GetDataRange(deviceid, type, &min_value, &max_value)) {
    *value = (*value - min_value) / (max_value - min_value);
    DCHECK(*value >= 0.0 && *value <= 1.0);
    return true;
  }
  return false;
}

bool DeviceDataManagerX11::GetDataRange(x11::Input::DeviceId deviceid,
                                        const DataType type,
                                        double* min,
                                        double* max) {
  auto device16 = static_cast<uint16_t>(deviceid);
  if (device16 >= kMaxDeviceNum)
    return false;
  if (valuator_lookup_[device16].empty())
    return false;
  if (valuator_lookup_[device16][type].number >= 0) {
    *min = valuator_lookup_[device16][type].min;
    *max = valuator_lookup_[device16][type].max;
    return true;
  }
  return false;
}

void DeviceDataManagerX11::SetDeviceListForTest(
    const std::vector<int>& touchscreen,
    const std::vector<int>& cmt_devices,
    const std::vector<int>& other_devices) {
  for (int i = 0; i < kMaxDeviceNum; ++i) {
    valuator_count_[i] = 0;
    valuator_lookup_[i].clear();
    data_type_lookup_[i].clear();
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[i][j].clear();
  }

  for (auto deviceid : touchscreen) {
    InitializeValuatorsForTest(deviceid, kTouchDataTypeStart, kTouchDataTypeEnd,
                               0, 1000);
  }

  cmt_devices_.reset();
  for (auto deviceid : cmt_devices) {
    cmt_devices_[deviceid] = true;
    touchpads_[deviceid] = true;
    InitializeValuatorsForTest(deviceid, kCMTDataTypeStart, kCMTDataTypeEnd,
                               -1000, 1000);
  }

  for (auto deviceid : other_devices) {
    InitializeValuatorsForTest(deviceid, kCMTDataTypeStart, kCMTDataTypeEnd,
                               -1000, 1000);
  }
}

void DeviceDataManagerX11::SetValuatorDataForTest(
    x11::Input::DeviceEvent* devev,
    DataType type,
    double value) {
  uint16_t device = static_cast<uint16_t>(devev->deviceid);
  int index = valuator_lookup_[device][type].number;
  CHECK(!IsXinputMaskSet(devev->valuator_mask.data(), index));
  CHECK(index >= 0 && index < valuator_count_[device]);
  SetXinputMask(devev->valuator_mask.data(), index);

  x11::Input::Fp3232* valuators = devev->axisvalues.data();
  for (int i = 0; i < index; ++i) {
    if (IsXinputMaskSet(devev->valuator_mask.data(), i))
      valuators++;
  }
  for (int i = DT_LAST_ENTRY - 1; i > valuators - devev->axisvalues.data();
       --i) {
    devev->axisvalues[i] = devev->axisvalues[i - 1];
  }

  *valuators = DoubleToFp3232(value);
}

void DeviceDataManagerX11::InitializeValuatorsForTest(int deviceid,
                                                      int start_valuator,
                                                      int end_valuator,
                                                      double min_value,
                                                      double max_value) {
  valuator_lookup_[deviceid].resize(DT_LAST_ENTRY);
  data_type_lookup_[deviceid].resize(DT_LAST_ENTRY, DT_LAST_ENTRY);
  for (int j = 0; j < kMaxSlotNum; j++)
    last_seen_valuator_[deviceid][j].resize(DT_LAST_ENTRY, 0);
  for (int j = start_valuator; j <= end_valuator; ++j) {
    auto& valuator_info = valuator_lookup_[deviceid][j];
    valuator_info.number = valuator_count_[deviceid];
    valuator_info.min = min_value;
    valuator_info.max = max_value;
    data_type_lookup_[deviceid][valuator_count_[deviceid]] = j;
    valuator_count_[deviceid]++;
  }
}

DeviceDataManagerX11::DataType DeviceDataManagerX11::UpdateValuatorClassDevice(
    const x11::Input::DeviceClass::Valuator& valuator_class_info,
    x11::Atom* atoms,
    uint16_t deviceid) {
  DCHECK_LT(deviceid, kMaxDeviceNum);
  x11::Atom* label =
      std::find(atoms, atoms + DT_LAST_ENTRY, valuator_class_info.label);
  if (label == atoms + DT_LAST_ENTRY)
    return DT_LAST_ENTRY;
  int data_type = label - atoms;
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type, DT_LAST_ENTRY);

  auto& valuator_info = valuator_lookup_[deviceid][data_type];
  valuator_info.number = valuator_class_info.number;
  valuator_info.min = Fp3232ToDouble(valuator_class_info.min);
  valuator_info.max = Fp3232ToDouble(valuator_class_info.max);
  data_type_lookup_[deviceid][valuator_class_info.number] = data_type;
  return static_cast<DataType>(data_type);
}

void DeviceDataManagerX11::UpdateScrollClassDevice(
    const x11::Input::DeviceClass::Scroll& scroll_class_info,
    uint16_t deviceid) {
  if (high_precision_scrolling_disabled_)
    return;

  DCHECK(deviceid >= 0 && deviceid < kMaxDeviceNum);
  ScrollInfo& info = scroll_data_[deviceid];
  switch (scroll_class_info.scroll_type) {
    case x11::Input::ScrollType::Vertical:
      info.vertical.number = scroll_class_info.number;
      info.vertical.increment = Fp3232ToDouble(scroll_class_info.increment);
      info.vertical.position = 0;
      info.vertical.seen = false;
      break;
    case x11::Input::ScrollType::Horizontal:
      info.horizontal.number = scroll_class_info.number;
      info.horizontal.increment = Fp3232ToDouble(scroll_class_info.increment);
      info.horizontal.position = 0;
      info.horizontal.seen = false;
      break;
  }
}

double DeviceDataManagerX11::ExtractAndUpdateScrollOffset(
    ScrollInfo::AxisInfo* axis,
    double valuator) const {
  double offset = 0;
  if (axis->seen)
    offset = axis->position - valuator;
  axis->seen = true;
  axis->position = valuator;
  return offset / axis->increment;
}

void DeviceDataManagerX11::SetDisabledKeyboardAllowedKeys(
    std::unique_ptr<std::set<KeyboardCode>> excepted_keys) {
  DCHECK(!excepted_keys.get() || !blocked_keyboard_allowed_keys_.get());
  blocked_keyboard_allowed_keys_ = std::move(excepted_keys);
}

void DeviceDataManagerX11::DisableDevice(x11::Input::DeviceId deviceid) {
  blocked_devices_.set(static_cast<uint32_t>(deviceid), true);
  // TODO(rsadam@): Support blocking touchscreen devices.
  std::vector<KeyboardDevice> keyboards = GetKeyboardDevices();
  auto it = FindDeviceWithId(keyboards.begin(), keyboards.end(), deviceid);
  if (it != std::end(keyboards)) {
    blocked_keyboard_devices_.emplace(deviceid, *it);
    keyboards.erase(it);
    DeviceDataManager::OnKeyboardDevicesUpdated(keyboards);
  }
}

void DeviceDataManagerX11::EnableDevice(x11::Input::DeviceId deviceid) {
  blocked_devices_.set(static_cast<uint32_t>(deviceid), false);
  auto it = blocked_keyboard_devices_.find(deviceid);
  if (it != blocked_keyboard_devices_.end()) {
    std::vector<KeyboardDevice> devices = GetKeyboardDevices();
    // Add device to current list of active devices.
    devices.push_back((*it).second);
    blocked_keyboard_devices_.erase(it);
    DeviceDataManager::OnKeyboardDevicesUpdated(devices);
  }
}

bool DeviceDataManagerX11::IsDeviceEnabled(
    x11::Input::DeviceId device_id) const {
  return blocked_devices_.test(static_cast<uint32_t>(device_id));
}

bool DeviceDataManagerX11::IsEventBlocked(const x11::Event& x11_event) {
  auto* xievent = x11_event.As<x11::Input::DeviceEvent>();
  if (!xievent)
    return false;
  // Allow any key events from blocked_keyboard_allowed_keys_.
  if (blocked_keyboard_allowed_keys_ &&
      (xievent->opcode == x11::Input::DeviceEvent::KeyPress ||
       xievent->opcode == x11::Input::DeviceEvent::KeyRelease) &&
      blocked_keyboard_allowed_keys_->find(KeyboardCodeFromXKeyEvent(
          x11_event)) != blocked_keyboard_allowed_keys_->end()) {
    return false;
  }

  return blocked_devices_.test(static_cast<uint16_t>(xievent->sourceid));
}

void DeviceDataManagerX11::OnKeyboardDevicesUpdated(
    const std::vector<KeyboardDevice>& devices) {
  std::vector<KeyboardDevice> keyboards(devices);
  for (auto blocked_iter = blocked_keyboard_devices_.begin();
       blocked_iter != blocked_keyboard_devices_.end();) {
    // Check if the blocked device still exists in list of devices.
    x11::Input::DeviceId device_id = blocked_iter->first;
    auto it = FindDeviceWithId(keyboards.begin(), keyboards.end(), device_id);
    // If the device no longer exists, unblock it, else filter it out from our
    // active list.
    if (it == keyboards.end()) {
      blocked_devices_.set(static_cast<uint32_t>((*blocked_iter).first), false);
      blocked_keyboard_devices_.erase(blocked_iter++);
    } else {
      keyboards.erase(it);
      ++blocked_iter;
    }
  }
  // Notify base class of updated list.
  DeviceDataManager::OnKeyboardDevicesUpdated(keyboards);
}

}  // namespace ui
