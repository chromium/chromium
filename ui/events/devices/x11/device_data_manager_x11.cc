// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/device_data_manager_x11.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

// XIScrollClass was introduced in XI 2.1 so we need to define it here
// for backward-compatibility with older versions of XInput.
#if !defined(XIScrollClass)
#define XIScrollClass 3
#endif

// Multi-touch support was introduced in XI 2.2. Add XI event types here
// for backward-compatibility with older versions of XInput.
#if !defined(XI_TouchBegin)
#define XI_TouchBegin  18
#define XI_TouchUpdate 19
#define XI_TouchEnd    20
#endif

// Copied from xserver-properties.h
#define AXIS_LABEL_PROP_REL_HWHEEL "Rel Horiz Wheel"
#define AXIS_LABEL_PROP_REL_WHEEL "Rel Vert Wheel"

// CMT specific timings
#define AXIS_LABEL_PROP_ABS_DBL_START_TIME "Abs Dbl Start Timestamp"
#define AXIS_LABEL_PROP_ABS_DBL_END_TIME   "Abs Dbl End Timestamp"

// Ordinal values
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X   "Abs Dbl Ordinal X"
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y   "Abs Dbl Ordinal Y"

// Fling properties
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VX   "Abs Dbl Fling X Velocity"
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VY   "Abs Dbl Fling Y Velocity"
#define AXIS_LABEL_PROP_ABS_FLING_STATE   "Abs Fling State"

#define AXIS_LABEL_PROP_ABS_FINGER_COUNT   "Abs Finger Count"

// Cros metrics gesture from touchpad
#define AXIS_LABEL_PROP_ABS_METRICS_TYPE      "Abs Metrics Type"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1 "Abs Dbl Metrics Data 1"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2 "Abs Dbl Metrics Data 2"

// Touchscreen multi-touch
#define AXIS_LABEL_ABS_MT_TOUCH_MAJOR "Abs MT Touch Major"
#define AXIS_LABEL_ABS_MT_TOUCH_MINOR "Abs MT Touch Minor"
#define AXIS_LABEL_ABS_MT_ORIENTATION "Abs MT Orientation"
#define AXIS_LABEL_ABS_MT_PRESSURE    "Abs MT Pressure"
#define AXIS_LABEL_ABS_MT_POSITION_X  "Abs MT Position X"
#define AXIS_LABEL_ABS_MT_POSITION_Y  "Abs MT Position Y"
#define AXIS_LABEL_ABS_MT_TRACKING_ID "Abs MT Tracking ID"
#define AXIS_LABEL_TOUCH_TIMESTAMP    "Touch Timestamp"

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
};

// Make sure the sizes of enum and |kCachedAtoms| are aligned.
static_assert(base::size(kCachedAtoms) ==
                  ui::DeviceDataManagerX11::DT_LAST_ENTRY,
              "kCachedAtoms count / enum mismatch");

// Constants for checking if a data type lies in the range of CMT/Touch data
// types.
const int kCMTDataTypeStart = ui::DeviceDataManagerX11::DT_CMT_SCROLL_X;
const int kCMTDataTypeEnd = ui::DeviceDataManagerX11::DT_CMT_FINGER_COUNT;
const int kTouchDataTypeStart = ui::DeviceDataManagerX11::DT_TOUCH_MAJOR;
const int kTouchDataTypeEnd = ui::DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP;

namespace ui {

namespace {

template <typename Iterator>
Iterator FindDeviceWithId(Iterator begin, Iterator end, int id) {
  for (auto it = begin; it != end; ++it) {
    if (it->id == id)
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

}  // namespace

bool DeviceDataManagerX11::IsCMTDataType(const int type) {
  return (type >= kCMTDataTypeStart) && (type <= kCMTDataTypeEnd);
}

bool DeviceDataManagerX11::IsTouchDataType(const int type) {
  return (type >= kTouchDataTypeStart) && (type <= kTouchDataTypeEnd);
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
    : xi_opcode_(-1),
      high_precision_scrolling_disabled_(IsHighPrecisionScrollingDisabled()),
      button_map_count_(0) {
  CHECK(gfx::GetXDisplay());
  InitializeXInputInternal();

  UpdateDeviceList(gfx::GetXDisplay());
  UpdateButtonMap();
}

DeviceDataManagerX11::~DeviceDataManagerX11() {
}

bool DeviceDataManagerX11::InitializeXInputInternal() {
  // Check if XInput is available on the system.
  xi_opcode_ = -1;
  int opcode, event, error;
  if (!XQueryExtension(
      gfx::GetXDisplay(), "XInputExtension", &opcode, &event, &error)) {
    VLOG(1) << "X Input extension not available: error=" << error;
    return false;
  }

  // Check the XInput version.
  int major = 2, minor = 2;
  if (XIQueryVersion(gfx::GetXDisplay(), &major, &minor) == BadRequest) {
    VLOG(1) << "XInput2 not supported in the server.";
    return false;
  }
  if (major < 2 || (major == 2 && minor < 2)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2.2 is required.";
    return false;
  }

  xi_opcode_ = opcode;
  CHECK_NE(-1, xi_opcode_);

  // Possible XI event types for XIDeviceEvent. See the XI2 protocol
  // specification.
  xi_device_event_types_[XI_KeyPress] = true;
  xi_device_event_types_[XI_KeyRelease] = true;
  xi_device_event_types_[XI_ButtonPress] = true;
  xi_device_event_types_[XI_ButtonRelease] = true;
  xi_device_event_types_[XI_Motion] = true;
  // Multi-touch support was introduced in XI 2.2.
  if (minor >= 2) {
    xi_device_event_types_[XI_TouchBegin] = true;
    xi_device_event_types_[XI_TouchUpdate] = true;
    xi_device_event_types_[XI_TouchEnd] = true;
  }
  return true;
}

bool DeviceDataManagerX11::IsXInput2Available() const {
  return xi_opcode_ != -1;
}

void DeviceDataManagerX11::UpdateDeviceList(Display* display) {
  cmt_devices_.reset();
  touchpads_.reset();
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
      ui::DeviceListCacheX11::GetInstance()->GetXDeviceList(display);
  Atom xi_touchpad = gfx::GetAtom(XI_TOUCHPAD);
  for (int i = 0; i < dev_list.count; ++i)
    if (dev_list[i].type == xi_touchpad)
      touchpads_[dev_list[i].id] = true;

  if (!IsXInput2Available())
    return;

  // Update the structs with new valuator information
  const XIDeviceList& info_list =
      ui::DeviceListCacheX11::GetInstance()->GetXI2DeviceList(display);
  Atom atoms[DT_LAST_ENTRY];
  for (int data_type = 0; data_type < DT_LAST_ENTRY; ++data_type)
    atoms[data_type] = gfx::GetAtom(kCachedAtoms[data_type]);

  for (int i = 0; i < info_list.count; ++i) {
    const XIDeviceInfo& info = info_list[i];

    if (info.use == XIMasterPointer)
      master_pointers_.push_back(info.deviceid);

    // We currently handle only slave, non-keyboard devices
    if (info.use != XISlavePointer && info.use != XIFloatingSlave)
      continue;

    bool possible_cmt = false;
    bool not_cmt = false;
    const int deviceid = info.deviceid;

    for (int j = 0; j < info.num_classes; ++j) {
      if (info.classes[j]->type == XIValuatorClass)
        ++valuator_count_[deviceid];
      else if (info.classes[j]->type == XIScrollClass)
        not_cmt = true;
    }

    // Skip devices that don't use any valuator
    if (!valuator_count_[deviceid])
      continue;

    valuator_lookup_[deviceid].resize(DT_LAST_ENTRY);
    data_type_lookup_[deviceid].resize(
        valuator_count_[deviceid], DT_LAST_ENTRY);
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[deviceid][j].resize(DT_LAST_ENTRY, 0);
    for (int j = 0; j < info.num_classes; ++j) {
      if (info.classes[j]->type == XIValuatorClass) {
        if (UpdateValuatorClassDevice(
                reinterpret_cast<XIValuatorClassInfo*>(info.classes[j]), atoms,
                deviceid))
          possible_cmt = true;
      } else if (info.classes[j]->type == XIScrollClass) {
        UpdateScrollClassDevice(
            reinterpret_cast<XIScrollClassInfo*>(info.classes[j]), deviceid);
      }
    }

    if (possible_cmt && !not_cmt)
      cmt_devices_[deviceid] = true;
  }
}

bool DeviceDataManagerX11::GetSlotNumber(const XIDeviceEvent* xiev, int* slot) {
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
  if (!factory->IsMultiTouchDevice(xiev->sourceid)) {
    *slot = 0;
    return true;
  }
  return factory->QuerySlotForTrackingID(xiev->detail, slot);
}

void DeviceDataManagerX11::GetEventRawData(const XEvent& xev, EventData* data) {
  if (xev.type != GenericEvent)
    return;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  CHECK_GE(xiev->sourceid, 0);
  CHECK_GE(xiev->deviceid, 0);
  if (xiev->sourceid >= kMaxDeviceNum || xiev->deviceid >= kMaxDeviceNum)
    return;
  data->clear();
  const int sourceid = xiev->sourceid;
  double* valuators = xiev->valuators.values;
  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (XIMaskIsSet(xiev->valuators.mask, i)) {
      int type = data_type_lookup_[sourceid][i];
      if (type != DT_LAST_ENTRY) {
        (*data)[type] = *valuators;
        if (IsTouchDataType(type)) {
          int slot = -1;
          if (GetSlotNumber(xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
            last_seen_valuator_[sourceid][slot][type] = *valuators;
        }
      }
      valuators++;
    }
  }
}

bool DeviceDataManagerX11::GetEventData(const XEvent& xev,
    const DataType type, double* value) {
  if (xev.type != GenericEvent)
    return false;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  CHECK_GE(xiev->sourceid, 0);
  CHECK_GE(xiev->deviceid, 0);
  if (xiev->sourceid >= kMaxDeviceNum || xiev->deviceid >= kMaxDeviceNum)
    return false;
  const int sourceid = xiev->sourceid;
  if (valuator_lookup_[sourceid].empty())
    return false;

  if (type == DT_TOUCH_TRACKING_ID) {
    // With XInput2 MT, Tracking ID is provided in the detail field for touch
    // events.
    if (xiev->evtype == XI_TouchBegin ||
        xiev->evtype == XI_TouchEnd ||
        xiev->evtype == XI_TouchUpdate) {
      *value = xiev->detail;
    } else {
      *value = 0;
    }
    return true;
  }

  int val_index = valuator_lookup_[sourceid][type].number;
  int slot = 0;
  if (val_index >= 0) {
    if (XIMaskIsSet(xiev->valuators.mask, val_index)) {
      double* valuators = xiev->valuators.values;
      while (val_index--) {
        if (XIMaskIsSet(xiev->valuators.mask, val_index))
          ++valuators;
      }
      *value = *valuators;
      if (IsTouchDataType(type)) {
        if (GetSlotNumber(xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
          last_seen_valuator_[sourceid][slot][type] = *value;
      }
      return true;
    } else if (IsTouchDataType(type)) {
      if (GetSlotNumber(xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
        *value = last_seen_valuator_[sourceid][slot][type];
    }
  }

  return false;
}

bool DeviceDataManagerX11::IsXIDeviceEvent(const XEvent& xev) const {
  if (xev.type != GenericEvent || xev.xcookie.extension != xi_opcode_)
    return false;
  return xi_device_event_types_[xev.xcookie.evtype];
}

bool DeviceDataManagerX11::IsTouchpadXInputEvent(const XEvent& xev) const {
  if (xev.type != GenericEvent)
    return false;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  CHECK_GE(xievent->sourceid, 0);
  if (xievent->sourceid >= kMaxDeviceNum)
    return false;
  return touchpads_[xievent->sourceid];
}

bool DeviceDataManagerX11::IsCMTDeviceEvent(const XEvent& xev) const {
  if (xev.type != GenericEvent)
    return false;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  CHECK_GE(xievent->sourceid, 0);
  if (xievent->sourceid >= kMaxDeviceNum)
    return false;
  return cmt_devices_[xievent->sourceid];
}

int DeviceDataManagerX11::GetScrollClassEventDetail(const XEvent& xev) const {
  if (xev.type != GenericEvent)
    return SCROLL_TYPE_NO_SCROLL;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xievent->sourceid >= kMaxDeviceNum)
    return SCROLL_TYPE_NO_SCROLL;
  int horizontal_id = scroll_data_[xievent->sourceid].horizontal.number;
  int vertical_id = scroll_data_[xievent->sourceid].vertical.number;
  return (horizontal_id != -1 &&
                  XIMaskIsSet(xievent->valuators.mask, horizontal_id)
              ? SCROLL_TYPE_HORIZONTAL
              : 0) |
         (vertical_id != -1 && XIMaskIsSet(xievent->valuators.mask, vertical_id)
              ? SCROLL_TYPE_VERTICAL
              : 0);
}

int DeviceDataManagerX11::GetScrollClassDeviceDetail(const XEvent& xev) const {
  if (xev.type != GenericEvent)
    return SCROLL_TYPE_NO_SCROLL;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum || xiev->deviceid >= kMaxDeviceNum)
    return SCROLL_TYPE_NO_SCROLL;
  const int sourceid = xiev->sourceid;
  const ScrollInfo& device_data = scroll_data_[sourceid];
  return (device_data.vertical.number >= 0 ? SCROLL_TYPE_VERTICAL : 0) |
         (device_data.horizontal.number >= 0 ? SCROLL_TYPE_HORIZONTAL : 0);
}

bool DeviceDataManagerX11::IsCMTGestureEvent(const XEvent& xev) const {
  return (IsScrollEvent(xev) || IsFlingEvent(xev) || IsCMTMetricsEvent(xev));
}

bool DeviceDataManagerX11::HasEventData(
    const XIDeviceEvent* xiev, const DataType type) const {
  CHECK_GE(xiev->sourceid, 0);
  if (xiev->sourceid >= kMaxDeviceNum)
    return false;
  if (type >= valuator_lookup_[xiev->sourceid].size())
    return false;
  const int idx = valuator_lookup_[xiev->sourceid][type].number;
  return (idx >= 0) && XIMaskIsSet(xiev->valuators.mask, idx);
}

bool DeviceDataManagerX11::IsScrollEvent(const XEvent& xev) const {
  if (!IsCMTDeviceEvent(xev))
    return false;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  return (HasEventData(xiev, DT_CMT_SCROLL_X) ||
          HasEventData(xiev, DT_CMT_SCROLL_Y));
}

bool DeviceDataManagerX11::IsFlingEvent(const XEvent& xev) const {
  if (!IsCMTDeviceEvent(xev))
    return false;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  return (HasEventData(xiev, DT_CMT_FLING_X) &&
          HasEventData(xiev, DT_CMT_FLING_Y) &&
          HasEventData(xiev, DT_CMT_FLING_STATE));
}

bool DeviceDataManagerX11::IsCMTMetricsEvent(const XEvent& xev) const {
  if (!IsCMTDeviceEvent(xev))
    return false;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  return (HasEventData(xiev, DT_CMT_METRICS_TYPE) &&
          HasEventData(xiev, DT_CMT_METRICS_DATA1) &&
          HasEventData(xiev, DT_CMT_METRICS_DATA2));
}

bool DeviceDataManagerX11::HasGestureTimes(const XEvent& xev) const {
  if (!IsCMTDeviceEvent(xev))
    return false;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  return (HasEventData(xiev, DT_CMT_START_TIME) &&
          HasEventData(xiev, DT_CMT_END_TIME));
}

void DeviceDataManagerX11::GetScrollOffsets(const XEvent& xev,
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

void DeviceDataManagerX11::GetScrollClassOffsets(const XEvent& xev,
                                                 double* x_offset,
                                                 double* y_offset) {
  DCHECK_NE(SCROLL_TYPE_NO_SCROLL, GetScrollClassDeviceDetail(xev));

  *x_offset = 0;
  *y_offset = 0;

  if (xev.type != GenericEvent)
    return;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum || xiev->deviceid >= kMaxDeviceNum)
    return;
  const int sourceid = xiev->sourceid;
  double* valuators = xiev->valuators.values;

  ScrollInfo* info = &scroll_data_[sourceid];

  const int horizontal_number = info->horizontal.number;
  const int vertical_number = info->vertical.number;

  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (!XIMaskIsSet(xiev->valuators.mask, i))
      continue;
    if (i == horizontal_number) {
      *x_offset = ExtractAndUpdateScrollOffset(&info->horizontal, *valuators);
    } else if (i == vertical_number) {
      *y_offset = ExtractAndUpdateScrollOffset(&info->vertical, *valuators);
    }
    valuators++;
  }
}

void DeviceDataManagerX11::InvalidateScrollClasses(int device_id) {
  if (device_id == kAllDevices) {
    for (int i = 0; i < kMaxDeviceNum; i++) {
      scroll_data_[i].horizontal.seen = false;
      scroll_data_[i].vertical.seen = false;
    }
  } else {
    CHECK(device_id >= 0 && device_id < kMaxDeviceNum);
    scroll_data_[device_id].horizontal.seen = false;
    scroll_data_[device_id].vertical.seen = false;
  }
}

void DeviceDataManagerX11::GetFlingData(const XEvent& xev,
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

void DeviceDataManagerX11::GetMetricsData(const XEvent& xev,
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
  return button > 0 && button <= button_map_count_ ? button_map_[button - 1] :
                                                     button;
}

void DeviceDataManagerX11::UpdateButtonMap() {
  button_map_count_ = XGetPointerMapping(gfx::GetXDisplay(), button_map_,
                                         base::size(button_map_));
}

void DeviceDataManagerX11::GetGestureTimes(const XEvent& xev,
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

bool DeviceDataManagerX11::NormalizeData(int deviceid,
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

bool DeviceDataManagerX11::GetDataRange(int deviceid,
                                        const DataType type,
                                        double* min,
                                        double* max) {
  CHECK_GE(deviceid, 0);
  if (deviceid >= kMaxDeviceNum)
    return false;
  if (valuator_lookup_[deviceid].empty())
    return false;
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
  for (int i = 0; i < kMaxDeviceNum; ++i) {
    valuator_count_[i] = 0;
    valuator_lookup_[i].clear();
    data_type_lookup_[i].clear();
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[i][j].clear();
  }

  for (int deviceid : touchscreen) {
    InitializeValuatorsForTest(deviceid, kTouchDataTypeStart, kTouchDataTypeEnd,
                               0, 1000);
  }

  cmt_devices_.reset();
  for (int deviceid : cmt_devices) {
    cmt_devices_[deviceid] = true;
    touchpads_[deviceid] = true;
    InitializeValuatorsForTest(deviceid, kCMTDataTypeStart, kCMTDataTypeEnd,
                               -1000, 1000);
  }

  for (int deviceid : other_devices) {
    InitializeValuatorsForTest(deviceid, kCMTDataTypeStart, kCMTDataTypeEnd,
                               -1000, 1000);
  }
}

void DeviceDataManagerX11::SetValuatorDataForTest(XIDeviceEvent* xievent,
                                                  DataType type,
                                                  double value) {
  int index = valuator_lookup_[xievent->deviceid][type].number;
  CHECK(!XIMaskIsSet(xievent->valuators.mask, index));
  CHECK(index >= 0 && index < valuator_count_[xievent->deviceid]);
  XISetMask(xievent->valuators.mask, index);

  double* valuators = xievent->valuators.values;
  for (int i = 0; i < index; ++i) {
    if (XIMaskIsSet(xievent->valuators.mask, i))
      valuators++;
  }
  for (int i = DT_LAST_ENTRY - 1; i > valuators - xievent->valuators.values;
       --i)
    xievent->valuators.values[i] = xievent->valuators.values[i - 1];
  *valuators = value;
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

bool DeviceDataManagerX11::UpdateValuatorClassDevice(
    XIValuatorClassInfo* valuator_class_info,
    Atom* atoms,
    int deviceid) {
  DCHECK(deviceid >= 0 && deviceid < kMaxDeviceNum);
  Atom* label =
      std::find(atoms, atoms + DT_LAST_ENTRY, valuator_class_info->label);
  if (label == atoms + DT_LAST_ENTRY) {
    return false;
  }
  int data_type = label - atoms;
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type, DT_LAST_ENTRY);

  auto& valuator_info = valuator_lookup_[deviceid][data_type];
  valuator_info.number = valuator_class_info->number;
  valuator_info.min = valuator_class_info->min;
  valuator_info.max = valuator_class_info->max;
  data_type_lookup_[deviceid][valuator_class_info->number] = data_type;
  return IsCMTDataType(data_type);
}

void DeviceDataManagerX11::UpdateScrollClassDevice(
    XIScrollClassInfo* scroll_class_info,
    int deviceid) {
  if (high_precision_scrolling_disabled_)
    return;

  DCHECK(deviceid >= 0 && deviceid < kMaxDeviceNum);
  ScrollInfo& info = scroll_data_[deviceid];

  bool legacy_scroll_available =
      (scroll_class_info->flags & XIScrollFlagNoEmulation) == 0;
  // If the device's highest resolution is lower than the resolution of xinput1
  // then use xinput1's events instead (ie. don't configure smooth scrolling).
  if (legacy_scroll_available &&
      std::abs(scroll_class_info->increment) <= 1.0) {
    return;
  }

  switch (scroll_class_info->scroll_type) {
    case XIScrollTypeVertical:
      info.vertical.number = scroll_class_info->number;
      info.vertical.increment = scroll_class_info->increment;
      info.vertical.position = 0;
      info.vertical.seen = false;
      break;
    case XIScrollTypeHorizontal:
      info.horizontal.number = scroll_class_info->number;
      info.horizontal.increment = scroll_class_info->increment;
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
  DCHECK(!excepted_keys.get() ||
         !blocked_keyboard_allowed_keys_.get());
  blocked_keyboard_allowed_keys_ = std::move(excepted_keys);
}

void DeviceDataManagerX11::DisableDevice(int deviceid) {
  blocked_devices_.set(deviceid, true);
  // TODO(rsadam@): Support blocking touchscreen devices.
  std::vector<InputDevice> keyboards = GetKeyboardDevices();
  auto it = FindDeviceWithId(keyboards.begin(), keyboards.end(), deviceid);
  if (it != std::end(keyboards)) {
    blocked_keyboard_devices_.insert(
        std::pair<int, InputDevice>(deviceid, *it));
    keyboards.erase(it);
    DeviceDataManager::OnKeyboardDevicesUpdated(keyboards);
  }
}

void DeviceDataManagerX11::EnableDevice(int deviceid) {
  blocked_devices_.set(deviceid, false);
  auto it = blocked_keyboard_devices_.find(deviceid);
  if (it != blocked_keyboard_devices_.end()) {
    std::vector<InputDevice> devices = GetKeyboardDevices();
    // Add device to current list of active devices.
    devices.push_back((*it).second);
    blocked_keyboard_devices_.erase(it);
    DeviceDataManager::OnKeyboardDevicesUpdated(devices);
  }
}

bool DeviceDataManagerX11::IsDeviceEnabled(int device_id) const {
  return blocked_devices_.test(device_id);
}

bool DeviceDataManagerX11::IsEventBlocked(const XEvent& xev) {
  // Only check XI2 events which have a source device id.
  if (xev.type != GenericEvent)
    return false;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  // Allow any key events from blocked_keyboard_allowed_keys_.
  if (blocked_keyboard_allowed_keys_ &&
      (xievent->evtype == XI_KeyPress || xievent->evtype == XI_KeyRelease) &&
      blocked_keyboard_allowed_keys_->find(KeyboardCodeFromXKeyEvent(&xev)) !=
          blocked_keyboard_allowed_keys_->end()) {
    return false;
  }

  return blocked_devices_.test(xievent->sourceid);
}

void DeviceDataManagerX11::OnKeyboardDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  std::vector<InputDevice> keyboards(devices);
  for (auto blocked_iter = blocked_keyboard_devices_.begin();
       blocked_iter != blocked_keyboard_devices_.end();) {
    // Check if the blocked device still exists in list of devices.
    int device_id = blocked_iter->first;
    auto it = FindDeviceWithId(keyboards.begin(), keyboards.end(), device_id);
    // If the device no longer exists, unblock it, else filter it out from our
    // active list.
    if (it == keyboards.end()) {
      blocked_devices_.set((*blocked_iter).first, false);
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
