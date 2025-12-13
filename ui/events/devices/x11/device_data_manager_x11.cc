// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/device_data_manager_x11.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
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

const struct {
  const char* name;
  DeviceDataManagerX11::DataType type;
} static constexpr kAtomNameToDataType[] = {
    {"Rel Horiz Wheel", DeviceDataManagerX11::DT_CMT_SCROLL_X},
    {"Rel Vert Wheel", DeviceDataManagerX11::DT_CMT_SCROLL_Y},
    {"Abs Dbl Ordinal X", DeviceDataManagerX11::DT_CMT_ORDINAL_X},
    {"Abs Dbl Ordinal Y", DeviceDataManagerX11::DT_CMT_ORDINAL_Y},
    {"Abs Dbl Start Timestamp", DeviceDataManagerX11::DT_CMT_START_TIME},
    {"Abs Dbl End Timestamp", DeviceDataManagerX11::DT_CMT_END_TIME},
    {"Abs Dbl Fling X Velocity", DeviceDataManagerX11::DT_CMT_FLING_X},
    {"Abs Dbl Fling Y Velocity", DeviceDataManagerX11::DT_CMT_FLING_Y},
    {"Abs Fling State", DeviceDataManagerX11::DT_CMT_FLING_STATE},
    {"Abs Metrics Type", DeviceDataManagerX11::DT_CMT_METRICS_TYPE},
    {"Abs Dbl Metrics Data 1", DeviceDataManagerX11::DT_CMT_METRICS_DATA1},
    {"Abs Dbl Metrics Data 2", DeviceDataManagerX11::DT_CMT_METRICS_DATA2},
    {"Abs Finger Count", DeviceDataManagerX11::DT_CMT_FINGER_COUNT},
    {"Abs MT Touch Major", DeviceDataManagerX11::DT_TOUCH_MAJOR},
    {"Abs MT Touch Minor", DeviceDataManagerX11::DT_TOUCH_MINOR},
    {"Abs MT Orientation", DeviceDataManagerX11::DT_TOUCH_ORIENTATION},
    {"Abs MT Pressure", DeviceDataManagerX11::DT_TOUCH_PRESSURE},
    {"Abs MT Position X", DeviceDataManagerX11::DT_TOUCH_POSITION_X},
    {"Abs MT Position Y", DeviceDataManagerX11::DT_TOUCH_POSITION_Y},
    {"Abs MT Tracking ID", DeviceDataManagerX11::DT_TOUCH_TRACKING_ID},
    {"Touch Timestamp", DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP},
    {"Abs X", DeviceDataManagerX11::DT_STYLUS_POSITION_X},
    {"Abs Y", DeviceDataManagerX11::DT_STYLUS_POSITION_Y},
    {"Abs Pressure", DeviceDataManagerX11::DT_STYLUS_PRESSURE},
    {"Abs Tilt X", DeviceDataManagerX11::DT_STYLUS_TILT_X},
    {"Abs Tilt Y", DeviceDataManagerX11::DT_STYLUS_TILT_Y},
};

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
  *sourceid = static_cast<uint16_t>(source);
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

  SelectDeviceEvents(x11::Input::XIEventMask::Hierarchy |
                     x11::Input::XIEventMask::DeviceChanged);

  UpdateDeviceList(x11::Connection::Get());
  UpdateButtonMap();
}

DeviceDataManagerX11::~DeviceDataManagerX11() {
  SelectDeviceEvents({});
}

bool DeviceDataManagerX11::IsXInput2Available() const {
  return x11::Connection::Get()->xinput_version() >=
         std::pair<uint32_t, uint32_t>{2, 2};
}

void DeviceDataManagerX11::UpdateDeviceList(x11::Connection* connection) {
  cmt_devices_.clear();
  touchpads_.clear();
  stylus_.clear();
  master_pointers_.clear();
  valuator_count_.clear();
  valuator_lookup_.clear();
  data_type_lookup_.clear();
  scroll_data_.clear();
  last_seen_valuator_.clear();

  // Find all the touchpad devices.
  const XDeviceList& dev_list =
      ui::DeviceListCacheX11::GetInstance()->GetXDeviceList(connection);
  x11::Atom xi_touchpad = x11::GetAtom("TOUCHPAD");
  for (const auto& device : dev_list) {
    if (device.device_type == xi_touchpad)
      touchpads_.insert(static_cast<x11::Input::DeviceId>(device.device_id));
  }

  if (!IsXInput2Available())
    return;

  // Update the structs with new valuator information
  const XIDeviceList& info_list =
      ui::DeviceListCacheX11::GetInstance()->GetXI2DeviceList(connection);
  base::flat_map<x11::Atom, DataType> atom_to_data_type;
  for (const auto& map_entry : kAtomNameToDataType) {
    atom_to_data_type[x11::GetAtom(map_entry.name)] = map_entry.type;
  }

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
    const auto deviceid = info.deviceid;

    for (const auto& device_class : info.classes) {
      if (device_class.valuator.has_value())
        ++valuator_count_[deviceid];
      else if (device_class.scroll.has_value())
        not_cmt = true;
    }

    // Skip devices that don't use any valuator
    if (!valuator_count_.contains(deviceid)) {
      continue;
    }

    valuator_lookup_[deviceid].resize(DT_LAST_ENTRY);
    data_type_lookup_[deviceid].resize(valuator_count_[deviceid],
                                       DT_LAST_ENTRY);
    for (const auto& device_class : info.classes) {
      if (device_class.valuator.has_value()) {
        auto dt = UpdateValuatorClassDevice(*device_class.valuator,
                                            atom_to_data_type, deviceid);
        if (IsStylusDataType(dt))
          possible_stylus = true;
        if (IsCMTDataType(dt))
          possible_cmt = true;
      } else if (device_class.scroll.has_value()) {
        UpdateScrollClassDevice(*device_class.scroll, deviceid);
      }
    }

    if (possible_stylus)
      stylus_.insert(deviceid);

    if (possible_cmt && !not_cmt)
      cmt_devices_.insert(deviceid);
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

  auto sourceid = xiev->sourceid;
  if (!valuator_count_.contains(sourceid)) {
    return;
  }
  data->clear();
  auto valuators_iter = xiev->axisvalues.begin();
  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (IsXinputMaskSet(xiev->valuator_mask.data(), i)) {
      int type = data_type_lookup_[sourceid][i];
      if (type != DT_LAST_ENTRY) {
        double valuator = Fp3232ToDouble(*valuators_iter);
        (*data)[type] = valuator;
        if (IsTouchDataType(type)) {
          int slot = -1;
          if (GetSlotNumber(*xiev, &slot) && slot >= 0) {
            auto& slot_valuators = last_seen_valuator_[sourceid][slot];
            if (slot_valuators.empty()) {
              slot_valuators.resize(DT_LAST_ENTRY, 0);
            }
            slot_valuators[type] = valuator;
          }
        }
      }
      ++valuators_iter;
    }
  }
}

bool DeviceDataManagerX11::GetEventData(const x11::Event& x11_event,
                                        const DataType type,
                                        double* value) {
  auto* xiev = x11_event.As<x11::Input::DeviceEvent>();
  if (!xiev)
    return false;

  auto sourceid = xiev->sourceid;
  if (!valuator_lookup_.contains(sourceid)) {
    return false;
  }

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
      auto valuators_iter = xiev->axisvalues.begin();
      while (val_index--) {
        if (IsXinputMaskSet(xiev->valuator_mask.data(), val_index))
          ++valuators_iter;
      }
      *value = Fp3232ToDouble(*valuators_iter);
      if (IsTouchDataType(type)) {
        if (GetSlotNumber(*xiev, &slot) && slot >= 0) {
          auto& slot_valuators = last_seen_valuator_[sourceid][slot];
          if (slot_valuators.empty()) {
            slot_valuators.resize(DT_LAST_ENTRY, 0);
          }
          slot_valuators[type] = *value;
        }
      }
      return true;
    } else if (IsTouchDataType(type)) {
      if (GetSlotNumber(*xiev, &slot) && slot >= 0) {
        if (last_seen_valuator_.contains(sourceid) &&
            last_seen_valuator_.at(sourceid).contains(slot)) {
          *value = last_seen_valuator_.at(sourceid).at(slot)[type];
        }
      }
    }
  }

  return false;
}

bool DeviceDataManagerX11::IsXIDeviceEvent(const x11::Event& x11_event) const {
  return x11_event.As<x11::Input::DeviceEvent>();
}

bool DeviceDataManagerX11::IsStylusXInputEvent(
    const x11::Event& x11_event) const {
  uint16_t source_id;
  if (!GetSourceId(x11_event, &source_id)) {
    return false;
  }
  return stylus_.contains(static_cast<x11::Input::DeviceId>(source_id));
}

bool DeviceDataManagerX11::IsTouchpadXInputEvent(
    const x11::Event& x11_event) const {
  uint16_t source_id;
  if (!GetSourceId(x11_event, &source_id)) {
    return false;
  }
  return touchpads_.contains(static_cast<x11::Input::DeviceId>(source_id));
}

bool DeviceDataManagerX11::IsCMTDeviceEvent(const x11::Event& x11_event) const {
  uint16_t source_id;
  if (!GetSourceId(x11_event, &source_id)) {
    return false;
  }
  return cmt_devices_.contains(static_cast<x11::Input::DeviceId>(source_id));
}

int DeviceDataManagerX11::GetScrollClassEventDetail(
    const x11::Event& x11_event) const {
  auto* xievent = x11_event.As<x11::Input::DeviceEvent>();
  if (!xievent)
    return SCROLL_TYPE_NO_SCROLL;
  auto sourceid = xievent->sourceid;
  if (!scroll_data_.contains(sourceid)) {
    return SCROLL_TYPE_NO_SCROLL;
  }
  int horizontal_id = scroll_data_.at(sourceid).horizontal.number;
  int vertical_id = scroll_data_.at(sourceid).vertical.number;
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

  auto sourceid = xiev->sourceid;
  if (!scroll_data_.contains(sourceid)) {
    return SCROLL_TYPE_NO_SCROLL;
  }
  const ScrollInfo& device_data = scroll_data_.at(sourceid);
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
  auto sourceid = xiev->sourceid;
  if (!valuator_lookup_.contains(sourceid)) {
    return false;
  }
  if (type >= valuator_lookup_.at(sourceid).size()) {
    return false;
  }
  const int idx = valuator_lookup_.at(sourceid)[type].number;
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

  auto sourceid = xiev->sourceid;
  if (!scroll_data_.contains(sourceid)) {
    return;
  }
  auto valuators_iter = xiev->axisvalues.begin();

  ScrollInfo* info = &scroll_data_[sourceid];

  const int horizontal_number = info->horizontal.number;
  const int vertical_number = info->vertical.number;

  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (!IsXinputMaskSet(xiev->valuator_mask.data(), i))
      continue;
    auto valuator = Fp3232ToDouble(*valuators_iter);
    if (i == horizontal_number)
      *x_offset = ExtractAndUpdateScrollOffset(&info->horizontal, valuator);
    else if (i == vertical_number)
      *y_offset = ExtractAndUpdateScrollOffset(&info->vertical, valuator);
    ++valuators_iter;
  }
}

void DeviceDataManagerX11::InvalidateScrollClasses(
    x11::Input::DeviceId device_id) {
  if (device_id == kAllDevices) {
    for (auto& i : scroll_data_) {
      i.second.horizontal.seen = false;
      i.second.vertical.seen = false;
    }
  } else {
    if (scroll_data_.contains(device_id)) {
      scroll_data_[device_id].horizontal.seen = false;
      scroll_data_[device_id].vertical.seen = false;
    }
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
  if (!valuator_lookup_.contains(deviceid)) {
    return false;
  }
  if (valuator_lookup_[deviceid][type].number >= 0) {
    *min = valuator_lookup_[deviceid][type].min;
    *max = valuator_lookup_[deviceid][type].max;
    return true;
  }
  return false;
}

void DeviceDataManagerX11::SetDeviceListForTest(
    const std::vector<int>& touchscreen,
    const std::vector<int>& cmt_devices,
    const std::vector<int>& other_devices) {
  valuator_count_.clear();
  valuator_lookup_.clear();
  data_type_lookup_.clear();
  last_seen_valuator_.clear();

  for (auto deviceid : touchscreen) {
    InitializeValuatorsForTest(static_cast<x11::Input::DeviceId>(deviceid),
                               kTouchDataTypeStart, kTouchDataTypeEnd, 0, 1000);
  }

  cmt_devices_.clear();
  for (auto deviceid : cmt_devices) {
    cmt_devices_.insert(static_cast<x11::Input::DeviceId>(deviceid));
    touchpads_.insert(static_cast<x11::Input::DeviceId>(deviceid));
    InitializeValuatorsForTest(static_cast<x11::Input::DeviceId>(deviceid),
                               kCMTDataTypeStart, kCMTDataTypeEnd, -1000, 1000);
  }

  for (auto deviceid : other_devices) {
    InitializeValuatorsForTest(static_cast<x11::Input::DeviceId>(deviceid),
                               kCMTDataTypeStart, kCMTDataTypeEnd, -1000, 1000);
  }
}

void DeviceDataManagerX11::SetValuatorDataForTest(
    x11::Input::DeviceEvent* devev,
    DataType type,
    double value) {
  auto device = devev->deviceid;
  int index = valuator_lookup_[device][type].number;
  CHECK(!IsXinputMaskSet(devev->valuator_mask.data(), index));
  CHECK(index >= 0 && index < valuator_count_[device]);
  SetXinputMask(devev->valuator_mask.data(), index);

  x11::Input::Fp3232* valuators = devev->axisvalues.data();
  for (int i = 0; i < index; ++i) {
    if (IsXinputMaskSet(devev->valuator_mask.data(), i))
      UNSAFE_TODO(valuators++);
  }
  for (int i = DT_LAST_ENTRY - 1; i > valuators - devev->axisvalues.data();
       --i) {
    devev->axisvalues[i] = devev->axisvalues[i - 1];
  }

  *valuators = DoubleToFp3232(value);
}

void DeviceDataManagerX11::InitializeValuatorsForTest(
    x11::Input::DeviceId deviceid,
    int start_valuator,
    int end_valuator,
    double min_value,
    double max_value) {
  valuator_lookup_[deviceid].resize(DT_LAST_ENTRY);
  data_type_lookup_[deviceid].resize(DT_LAST_ENTRY, DT_LAST_ENTRY);
  last_seen_valuator_[deviceid].clear();
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
    const base::flat_map<x11::Atom, DataType>& atom_to_data_type,
    x11::Input::DeviceId deviceid) {
  auto it = atom_to_data_type.find(valuator_class_info.label);
  if (it == atom_to_data_type.end()) {
    return DT_LAST_ENTRY;
  }
  int data_type = it->second;

  auto& valuator_info = valuator_lookup_[deviceid][data_type];
  valuator_info.number = valuator_class_info.number;
  valuator_info.min = Fp3232ToDouble(valuator_class_info.min);
  valuator_info.max = Fp3232ToDouble(valuator_class_info.max);
  data_type_lookup_[deviceid][valuator_class_info.number] = data_type;
  return static_cast<DataType>(data_type);
}

void DeviceDataManagerX11::UpdateScrollClassDevice(
    const x11::Input::DeviceClass::Scroll& scroll_class_info,
    x11::Input::DeviceId deviceid) {
  if (high_precision_scrolling_disabled_)
    return;

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
  blocked_devices_.insert(deviceid);
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
  blocked_devices_.erase(deviceid);
  auto it = blocked_keyboard_devices_.find(deviceid);
  if (it != blocked_keyboard_devices_.end()) {
    std::vector<KeyboardDevice> devices = GetKeyboardDevices();
    // Add device to current list of active devices.
    devices.push_back((*it).second);
    blocked_keyboard_devices_.erase(it);
    DeviceDataManager::OnKeyboardDevicesUpdated(devices);
  }
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

  return blocked_devices_.contains(xievent->sourceid);
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
      blocked_devices_.erase((*blocked_iter).first);
      blocked_keyboard_devices_.erase(blocked_iter++);
    } else {
      keyboards.erase(it);
      ++blocked_iter;
    }
  }
  // Notify base class of updated list.
  DeviceDataManager::OnKeyboardDevicesUpdated(keyboards);
}

void DeviceDataManagerX11::SelectDeviceEvents(
    x11::Input::XIEventMask event_mask) {
  auto* connection = x11::Connection::Get();
  x11::Input::EventMask mask{x11::Input::DeviceId::All, {event_mask}};
  connection->xinput().XISelectEvents({connection->default_root(), {mask}});
}

}  // namespace ui
