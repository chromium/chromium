// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/touch_factory_x11.h"

#include <stddef.h>

#include <string_view>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace ui {
namespace {

void AddPointerDevicesFromString(
    const std::string& pointer_devices,
    EventPointerType type,
    std::vector<std::pair<int, EventPointerType>>* devices) {
  for (std::string_view dev : base::SplitStringPiece(
           pointer_devices, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int devid;
    if (base::StringToInt(dev, &devid))
      devices->push_back({devid, type});
    else
      DLOG(WARNING) << "Invalid device id: " << dev;
  }
}

}  // namespace

TouchFactory::TouchFactory()
    : pointer_device_lookup_(),
      touch_device_list_(),
      id_generator_(0),
      touch_screens_enabled_(true) {
  // Ensure device data manager is properly initialized.
  DeviceDataManagerX11::CreateInstance();

  if (!DeviceDataManagerX11::GetInstance()->IsXInput2Available())
    return;

  UpdateDeviceList(x11::Connection::Get());
}

TouchFactory::~TouchFactory() = default;

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
      EventPointerType::kTouch, &devices);
  AddPointerDevicesFromString(
      command_line->GetSwitchValueASCII(switches::kPenDevices),
      EventPointerType::kPen, &devices);
  if (!devices.empty())
    ui::TouchFactory::GetInstance()->SetTouchDeviceList(devices);
}

void TouchFactory::UpdateDeviceList(x11::Connection* connection) {
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
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(connection);
  for (const auto& devinfo : xi_dev_list) {
    if (devinfo.type == x11::Input::DeviceType::FloatingSlave ||
        devinfo.type == x11::Input::DeviceType::MasterPointer ||
        devinfo.type == x11::Input::DeviceType::SlavePointer) {
      for (const auto& xiclassinfo : devinfo.classes) {
        if (!xiclassinfo.touch.has_value())
          continue;

        auto& tci = *xiclassinfo.touch;
        // Only care direct touch device (such as touch screen) right now
        if (tci.mode != x11::Input::TouchMode::Direct)
          continue;

        auto master_id = devinfo.type == x11::Input::DeviceType::SlavePointer
                             ? devinfo.attachment
                             : devinfo.deviceid;

        if (!IsValidDevice(static_cast<uint16_t>(master_id)))
          continue;

        touch_device_lookup_[static_cast<uint16_t>(master_id)] = true;
        touch_device_list_[master_id] = {true, EventPointerType::kTouch};

        if (devinfo.type != x11::Input::DeviceType::MasterPointer)
          CacheTouchscreenIds(devinfo.deviceid);

        if (devinfo.type == x11::Input::DeviceType::MasterPointer) {
          device_master_id_list_[devinfo.deviceid] = master_id;
          touch_device_lookup_[static_cast<uint16_t>(devinfo.deviceid)] = true;
          touch_device_list_[devinfo.deviceid] = {false,
                                                  EventPointerType::kTouch};
        }
      }
      pointer_device_lookup_[static_cast<uint16_t>(devinfo.deviceid)] =
          (devinfo.type != x11::Input::DeviceType::SlavePointer);
    } else if (devinfo.type == x11::Input::DeviceType::MasterKeyboard) {
      virtual_core_keyboard_device_ = devinfo.deviceid;
    }
  }
}

bool TouchFactory::ShouldProcessDeviceEvent(
    const x11::Input::DeviceEvent& xiev) {
  const bool is_touch_disabled = !touch_screens_enabled_;

  if (xiev.opcode == x11::Input::DeviceEvent::TouchBegin ||
      xiev.opcode == x11::Input::DeviceEvent::TouchUpdate ||
      xiev.opcode == x11::Input::DeviceEvent::TouchEnd) {
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
    bool is_from_master_or_float = touch_device_list_[xiev.deviceid].is_master;
    bool is_from_slave_device =
        !is_from_master_or_float && xiev.sourceid == xiev.deviceid;
    return !is_touch_disabled && IsTouchDevice(xiev.deviceid) &&
           !is_from_slave_device;
  }

  // Make sure only key-events from the virtual core keyboard are processed.
  if (xiev.opcode == x11::Input::DeviceEvent::KeyPress ||
      xiev.opcode == x11::Input::DeviceEvent::KeyRelease) {
    return !virtual_core_keyboard_device_.has_value() ||
           *virtual_core_keyboard_device_ == xiev.deviceid;
  }

  return ShouldProcessEventForDevice(xiev.deviceid);
}

bool TouchFactory::ShouldProcessCrossingEvent(
    const x11::Input::CrossingEvent& xiev) {
  // Don't automatically accept x11::Input::CrossingEvent::Enter or
  // x11::Input::CrossingEvent::Leave. They should be checked against the
  // pointer_device_lookup_ to prevent handling for slave devices. This happens
  // for unknown reasons when using xtest. https://crbug.com/683434.
  if (xiev.opcode != x11::Input::CrossingEvent::Enter &&
      xiev.opcode != x11::Input::CrossingEvent::Leave) {
    return true;
  }

  return ShouldProcessEventForDevice(xiev.deviceid);
}

void TouchFactory::SetupXI2ForXWindow(x11::Window window) {
  // Setup mask for mouse events. It is possible that a device is loaded/plugged
  // in after we have setup XInput2 on a window. In such cases, we need to
  // either resetup XInput2 for the window, so that we get events from the new
  // device, or we need to listen to events from all devices, and then filter
  // the events from uninteresting devices. We do the latter because that's
  // simpler.

  auto* connection = x11::Connection::Get();

  x11::Input::EventMask mask{};
  mask.mask.push_back({});
  auto* mask_data = mask.mask.data();

  SetXinputMask(mask_data, x11::Input::CrossingEvent::Enter);
  SetXinputMask(mask_data, x11::Input::CrossingEvent::Leave);
  SetXinputMask(mask_data, x11::Input::CrossingEvent::FocusIn);
  SetXinputMask(mask_data, x11::Input::CrossingEvent::FocusOut);

  SetXinputMask(mask_data, x11::Input::DeviceEvent::TouchBegin);
  SetXinputMask(mask_data, x11::Input::DeviceEvent::TouchUpdate);
  SetXinputMask(mask_data, x11::Input::DeviceEvent::TouchEnd);

  SetXinputMask(mask_data, x11::Input::DeviceEvent::ButtonPress);
  SetXinputMask(mask_data, x11::Input::DeviceEvent::ButtonRelease);
  SetXinputMask(mask_data, x11::Input::DeviceEvent::Motion);
  // HierarchyChanged and DeviceChanged allow X11EventSource to still pick up
  // these events.
  SetXinputMask(mask_data, x11::Input::HierarchyEvent::opcode);
  SetXinputMask(mask_data, x11::Input::DeviceChangedEvent::opcode);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    SetXinputMask(mask_data, x11::Input::DeviceEvent::KeyPress);
    SetXinputMask(mask_data, x11::Input::DeviceEvent::KeyRelease);
  }
#endif

  connection->xinput().XISelectEvents({window, {mask}});
  connection->Flush();
}

void TouchFactory::SetTouchDeviceList(
    const std::vector<std::pair<int, EventPointerType>>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (auto& device : devices) {
    int device_int = device.first;
    auto device_id = static_cast<x11::Input::DeviceId>(device_int);
    EventPointerType type = device.second;
    DCHECK(IsValidDevice(device_int));
    touch_device_lookup_[device_int] = true;
    touch_device_list_[device_id] = {false, type};
    if (device_master_id_list_.find(device_id) !=
        device_master_id_list_.end()) {
      // When we set the device through the "--touch-devices" flag to slave
      // touch device, we also set its master device to be touch device.
      touch_device_lookup_[static_cast<uint16_t>(
          device_master_id_list_[device_id])] = true;
      touch_device_list_[device_master_id_list_[device_id]] = {false, type};
    }
  }
}

bool TouchFactory::IsValidDevice(int deviceid) const {
  return deviceid >= 0 &&
         static_cast<size_t>(deviceid) < touch_device_lookup_.size();
}

bool TouchFactory::IsTouchDevice(x11::Input::DeviceId deviceid) const {
  return IsValidDevice(static_cast<uint16_t>(deviceid)) &&
         touch_device_lookup_[static_cast<uint16_t>(deviceid)];
}

bool TouchFactory::IsMultiTouchDevice(x11::Input::DeviceId deviceid) const {
  return IsValidDevice(static_cast<uint16_t>(deviceid)) &&
         touch_device_lookup_[static_cast<uint16_t>(deviceid)] &&
         touch_device_list_.find(deviceid)->second.is_master;
}

EventPointerType TouchFactory::GetTouchDevicePointerType(
    x11::Input::DeviceId deviceid) const {
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
  return touch_screens_enabled_ && touch_device_lookup_.any();
}

void TouchFactory::ResetForTest() {
  pointer_device_lookup_.reset();
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  touchscreen_ids_.clear();
  id_generator_.ResetForTest();
  SetTouchscreensEnabled(true);
}

void TouchFactory::SetTouchDeviceForTest(const std::vector<int>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (int device_id : devices) {
    auto device = static_cast<x11::Input::DeviceId>(device_id);
    DCHECK(IsValidDevice(device_id));
    touch_device_lookup_[device_id] = true;
    touch_device_list_[device] = {true, EventPointerType::kTouch};
  }
  SetTouchscreensEnabled(true);
}

void TouchFactory::SetPointerDeviceForTest(const std::vector<int>& devices) {
  pointer_device_lookup_.reset();
  for (int device : devices)
    pointer_device_lookup_[device] = true;
}

void TouchFactory::SetTouchscreensEnabled(bool enabled) {
  touch_screens_enabled_ = enabled;
  DeviceDataManager::GetInstance()->SetTouchscreensEnabled(enabled);
}

void TouchFactory::CacheTouchscreenIds(x11::Input::DeviceId device_id) {
  if (!DeviceDataManager::HasInstance())
    return;
  std::vector<TouchscreenDevice> touchscreens =
      DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  const auto it = base::ranges::find(touchscreens, static_cast<int>(device_id),
                                     &TouchscreenDevice::id);
  // Internal displays will have a vid and pid of 0. Ignore them.
  if (it != touchscreens.end() && it->vendor_id && it->product_id)
    touchscreen_ids_.emplace(it->vendor_id, it->product_id);
}

bool TouchFactory::ShouldProcessEventForDevice(
    x11::Input::DeviceId device_id) const {
  if (!pointer_device_lookup_[static_cast<uint16_t>(device_id)])
    return false;

  return IsTouchDevice(device_id) ? touch_screens_enabled_ : true;
}

}  // namespace ui
