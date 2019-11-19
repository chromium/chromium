// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/touch_factory_x11.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/x/x11.h"

namespace ui {
namespace {

void AddPointerDevicesFromString(
    const std::string& pointer_devices,
    EventPointerType type,
    std::vector<std::pair<int, EventPointerType>>* devices) {
  for (const base::StringPiece& dev : base::SplitStringPiece(
           pointer_devices, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int devid;
    if (base::StringToInt(dev, &devid))
      devices->push_back({devid, type});
    else
      DLOG(WARNING) << "Invalid device id: " << dev.as_string();
  }
}

}  // namespace

TouchFactory::TouchFactory()
    : pointer_device_lookup_(),
      touch_device_list_(),
      virtual_core_keyboard_device_(-1),
      id_generator_(0),
      touch_screens_enabled_(true) {
  // Ensure device data manager is properly initialized.
  DeviceDataManagerX11::CreateInstance();

  if (!DeviceDataManagerX11::GetInstance()->IsXInput2Available())
    return;

  XDisplay* display = gfx::GetXDisplay();
  UpdateDeviceList(display);
}

TouchFactory::~TouchFactory() {
}

// static
TouchFactory* TouchFactory::GetInstance() {
  return base::Singleton<TouchFactory>::get();
}

// static
void TouchFactory::SetTouchDeviceListFromCommandLine() {
  // Get a list of pointer-devices that should be treated as touch-devices.
  // This is primarily used for testing/debugging touch-event processing when a
  // touch-device isn't available.
  std::vector<std::pair<int, EventPointerType>> devices;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddPointerDevicesFromString(
      command_line->GetSwitchValueASCII(switches::kTouchDevices),
      EventPointerType::POINTER_TYPE_TOUCH, &devices);
  AddPointerDevicesFromString(
      command_line->GetSwitchValueASCII(switches::kPenDevices),
      EventPointerType::POINTER_TYPE_PEN, &devices);
  if (!devices.empty())
    ui::TouchFactory::GetInstance()->SetTouchDeviceList(devices);
}

void TouchFactory::UpdateDeviceList(XDisplay* display) {
  // Detect touch devices.
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  touchscreen_ids_.clear();

  if (!DeviceDataManagerX11::GetInstance()->IsXInput2Available())
    return;

  // Instead of asking X for the list of devices all the time, let's maintain a
  // list of pointer devices we care about.
  // It should not be necessary to select for slave devices. XInput2 provides
  // enough information to the event callback to decide which slave device
  // triggered the event, thus decide whether the 'pointer event' is a
  // 'mouse event' or a 'touch event'.
  // However, on some desktops, some events from a master pointer are
  // not delivered to the client. So we select for slave devices instead.
  // If the touch device has 'GrabDevice' set and 'SendCoreEvents' unset (which
  // is possible), then the device is detected as a floating device, and a
  // floating device is not connected to a master device. So it is necessary to
  // also select on the floating devices.
  pointer_device_lookup_.reset();
  const XIDeviceList& xi_dev_list =
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(display);
  for (int i = 0; i < xi_dev_list.count; i++) {
    const XIDeviceInfo& devinfo = xi_dev_list[i];
    if (devinfo.use == XIFloatingSlave || devinfo.use == XIMasterPointer ||
        devinfo.use == XISlavePointer) {
      for (int k = 0; k < devinfo.num_classes; ++k) {
        XIAnyClassInfo* xiclassinfo = devinfo.classes[k];
        if (xiclassinfo->type != XITouchClass)
          continue;

        XITouchClassInfo* tci =
            reinterpret_cast<XITouchClassInfo*>(xiclassinfo);
        // Only care direct touch device (such as touch screen) right now
        if (tci->mode != XIDirectTouch)
          continue;

        int master_id = devinfo.use == XISlavePointer ? devinfo.attachment
                                                      : devinfo.deviceid;

        if (!IsValidDevice(master_id))
          continue;

        touch_device_lookup_[master_id] = true;
        touch_device_list_[master_id] = {true,
                                         EventPointerType::POINTER_TYPE_TOUCH};

        if (devinfo.use != XIMasterPointer)
          CacheTouchscreenIds(devinfo.deviceid);

        if (devinfo.use == XISlavePointer) {
          device_master_id_list_[devinfo.deviceid] = master_id;
          touch_device_lookup_[devinfo.deviceid] = true;
          touch_device_list_[devinfo.deviceid] = {
              false, EventPointerType::POINTER_TYPE_TOUCH};
        }
      }
      pointer_device_lookup_[devinfo.deviceid] =
          (devinfo.use != XISlavePointer);
    } else if (devinfo.use == XIMasterKeyboard) {
      virtual_core_keyboard_device_ = devinfo.deviceid;
    }
  }
}

bool TouchFactory::ShouldProcessXI2Event(XEvent* xev) {
  DCHECK_EQ(GenericEvent, xev->type);
  XIEvent* event = static_cast<XIEvent*>(xev->xcookie.data);
  XIDeviceEvent* xiev = reinterpret_cast<XIDeviceEvent*>(event);

  const bool is_touch_disabled = !touch_screens_enabled_;

  if (event->evtype == XI_TouchBegin ||
      event->evtype == XI_TouchUpdate ||
      event->evtype == XI_TouchEnd) {
    // Since SetupXI2ForXWindow() selects events from all devices, for a
    // touchscreen attached to a master pointer device, X11 sends two
    // events for each touch: one from the slave (deviceid == the id of
    // the touchscreen device), and one from the master (deviceid == the
    // id of the master pointer device). Instead of processing both
    // events, discard the event that comes from the slave, and only
    // allow processing the event coming from the master.
    // For a 'floating' touchscreen device, X11 sends only one event for
    // each touch, with both deviceid and sourceid set to the id of the
    // touchscreen device.
    bool is_from_master_or_float = touch_device_list_[xiev->deviceid].is_master;
    bool is_from_slave_device = !is_from_master_or_float
        && xiev->sourceid == xiev->deviceid;
    return !is_touch_disabled &&
           IsTouchDevice(xiev->deviceid) &&
           !is_from_slave_device;
  }

  // Make sure only key-events from the virtual core keyboard are processed.
  if (event->evtype == XI_KeyPress || event->evtype == XI_KeyRelease) {
    return (virtual_core_keyboard_device_ < 0) ||
           (virtual_core_keyboard_device_ == xiev->deviceid);
  }

  // Don't automatically accept XI_Enter or XI_Leave. They should be checked
  // against the pointer_device_lookup_ to prevent handling for slave devices.
  // This happens for unknown reasons when using xtest.
  // https://crbug.com/683434.
  if (event->evtype != XI_ButtonPress && event->evtype != XI_ButtonRelease &&
      event->evtype != XI_Enter && event->evtype != XI_Leave &&
      event->evtype != XI_Motion) {
    return true;
  }

  if (!pointer_device_lookup_[xiev->deviceid])
    return false;

  return IsTouchDevice(xiev->deviceid) ? !is_touch_disabled : true;
}

void TouchFactory::SetupXI2ForXWindow(Window window) {
  // Setup mask for mouse events. It is possible that a device is loaded/plugged
  // in after we have setup XInput2 on a window. In such cases, we need to
  // either resetup XInput2 for the window, so that we get events from the new
  // device, or we need to listen to events from all devices, and then filter
  // the events from uninteresting devices. We do the latter because that's
  // simpler.

  XDisplay* display = gfx::GetXDisplay();

  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_Enter);
  XISetMask(mask, XI_Leave);
  XISetMask(mask, XI_FocusIn);
  XISetMask(mask, XI_FocusOut);

  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);

  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);
  // HierarchyChanged and DeviceChanged allow X11EventSource to still pick up
  // these events.
  XISetMask(mask, XI_HierarchyChanged);
  XISetMask(mask, XI_DeviceChanged);
#if defined(OS_CHROMEOS)
  // XGrabKey() must be replaced with XI2 keyboard grab if XI2 key events are
  // enabled on desktop Linux.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    XISetMask(mask, XI_KeyPress);
    XISetMask(mask, XI_KeyRelease);
  }
#endif

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(display, window, &evmask, 1);
  XFlush(display);
}

void TouchFactory::SetTouchDeviceList(
    const std::vector<std::pair<int, EventPointerType>>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (auto& device : devices) {
    int deviceid = device.first;
    EventPointerType type = device.second;
    DCHECK(IsValidDevice(deviceid));
    touch_device_lookup_[deviceid] = true;
    touch_device_list_[deviceid] = {false, type};
    if (device_master_id_list_.find(deviceid) != device_master_id_list_.end()) {
      // When we set the device through the "--touch-devices" flag to slave
      // touch device, we also set its master device to be touch device.
      touch_device_lookup_[device_master_id_list_[deviceid]] = true;
      touch_device_list_[device_master_id_list_[deviceid]] = {false, type};
    }
  }
}

bool TouchFactory::IsValidDevice(int deviceid) const {
  return (deviceid >= 0) &&
         (static_cast<size_t>(deviceid) < touch_device_lookup_.size());
}

bool TouchFactory::IsTouchDevice(int deviceid) const {
  return IsValidDevice(deviceid) ? touch_device_lookup_[deviceid] : false;
}

bool TouchFactory::IsMultiTouchDevice(int deviceid) const {
  return (IsValidDevice(deviceid) && touch_device_lookup_[deviceid])
             ? touch_device_list_.find(deviceid)->second.is_master
             : false;
}

EventPointerType TouchFactory::GetTouchDevicePointerType(int deviceid) const {
  DCHECK(IsTouchDevice(deviceid));
  return touch_device_list_.find(deviceid)->second.pointer_type;
}

bool TouchFactory::QuerySlotForTrackingID(uint32_t tracking_id, int* slot) {
  if (!id_generator_.HasGeneratedIDFor(tracking_id))
    return false;
  *slot = GetSlotForTrackingID(tracking_id);
  return true;
}

int TouchFactory::GetSlotForTrackingID(uint32_t tracking_id) {
  return id_generator_.GetGeneratedID(tracking_id);
}

void TouchFactory::ReleaseSlot(int slot) {
  id_generator_.ReleaseID(slot);
}

bool TouchFactory::IsTouchDevicePresent() {
  return touch_screens_enabled_ &&
      touch_device_lookup_.any();
}

void TouchFactory::ResetForTest() {
  pointer_device_lookup_.reset();
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  touchscreen_ids_.clear();
  id_generator_.ResetForTest();
  SetTouchscreensEnabled(true);
}

void TouchFactory::SetTouchDeviceForTest(
    const std::vector<int>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (auto iter = devices.begin(); iter != devices.end(); ++iter) {
    DCHECK(IsValidDevice(*iter));
    touch_device_lookup_[*iter] = true;
    touch_device_list_[*iter] = {true, EventPointerType::POINTER_TYPE_TOUCH};
  }
  SetTouchscreensEnabled(true);
}

void TouchFactory::SetPointerDeviceForTest(
    const std::vector<int>& devices) {
  pointer_device_lookup_.reset();
  for (auto iter = devices.begin(); iter != devices.end(); ++iter) {
    pointer_device_lookup_[*iter] = true;
  }
}

void TouchFactory::SetTouchscreensEnabled(bool enabled) {
  touch_screens_enabled_ = enabled;
  DeviceDataManager::GetInstance()->SetTouchscreensEnabled(enabled);
}

void TouchFactory::CacheTouchscreenIds(int device_id) {
  if (!DeviceDataManager::HasInstance())
    return;
  std::vector<TouchscreenDevice> touchscreens =
      DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  const auto it =
      std::find_if(touchscreens.begin(), touchscreens.end(),
                   [device_id](const TouchscreenDevice& touchscreen) {
                     return touchscreen.id == device_id;
                   });
  // Internal displays will have a vid and pid of 0. Ignore them.
  if (it != touchscreens.end() && it->vendor_id && it->product_id)
    touchscreen_ids_.emplace(it->vendor_id, it->product_id);
}

}  // namespace ui
