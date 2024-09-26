// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/device/udev/device_manager_udev.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "device/udev_linux/udev.h"
#include "device/udev_linux/udev_loader.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"

namespace ui {

namespace {

const char* const kSubsystems[] = {
  "input",
  "drm",
};

// Start monitoring input device changes.
device::ScopedUdevMonitorPtr UdevCreateMonitor(struct udev* udev) {
  struct udev_monitor* monitor =
      device::udev_monitor_new_from_netlink(udev, "udev");
  if (monitor) {
    for (size_t i = 0; i < std::size(kSubsystems); ++i)
      device::udev_monitor_filter_add_match_subsystem_devtype(
          monitor, kSubsystems[i], nullptr);

    if (device::udev_monitor_enable_receiving(monitor))
      LOG(ERROR) << "Failed to start receiving events from udev";
  } else {
    LOG(ERROR) << "Failed to create udev monitor";
  }

  return device::ScopedUdevMonitorPtr(monitor);
}

}  // namespace

DeviceManagerUdev::DeviceManagerUdev()
    : udev_(device::udev_new()), controller_(FROM_HERE) {}

DeviceManagerUdev::~DeviceManagerUdev() {
}

void DeviceManagerUdev::CreateMonitor() {
  if (monitor_)
    return;
  monitor_ = UdevCreateMonitor(udev_.get());
  if (monitor_) {
    int fd = device::udev_monitor_get_fd(monitor_.get());
    CHECK_GT(fd, 0);
    base::CurrentUIThread::Get()->WatchFileDescriptor(
        fd, true, base::MessagePumpForUI::WATCH_READ, &controller_, this);
  }
}

void DeviceManagerUdev::ScanDevices(DeviceEventObserver* observer) {
  CreateMonitor();

  device::ScopedUdevEnumeratePtr enumerate(
      device::udev_enumerate_new(udev_.get()));
  if (!enumerate)
    return;

  for (size_t i = 0; i < std::size(kSubsystems); ++i)
    device::udev_enumerate_add_match_subsystem(enumerate.get(), kSubsystems[i]);
  device::udev_enumerate_scan_devices(enumerate.get());

  struct udev_list_entry* devices =
      device::udev_enumerate_get_list_entry(enumerate.get());
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, devices) {
    device::ScopedUdevDevicePtr device(device::udev_device_new_from_syspath(
        udev_.get(), device::udev_list_entry_get_name(entry)));
    if (!device)
      continue;

    std::unique_ptr<DeviceEvent> event = ProcessMessage(device.get());
    if (event)
      observer->OnDeviceEvent(*event.get());
  }
}

void DeviceManagerUdev::AddObserver(DeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void DeviceManagerUdev::RemoveObserver(DeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceManagerUdev::OnFileCanReadWithoutBlocking(int fd) {
  // The netlink socket should never become disconnected. There's no need
  // to handle broken connections here.
  TRACE_EVENT1("evdev", "UdevDeviceChange", "socket", fd);

  device::ScopedUdevDevicePtr device(
      device::udev_monitor_receive_device(monitor_.get()));
  if (!device)
    return;

  std::unique_ptr<DeviceEvent> event = ProcessMessage(device.get());
  if (event) {
    observers_.Notify(&DeviceEventObserver::OnDeviceEvent, *event.get());
  }
}

void DeviceManagerUdev::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<DeviceEvent> DeviceManagerUdev::ProcessMessage(
    udev_device* device) {
  const char* path = device::udev_device_get_devnode(device);
  const char* subsystem =
      device::udev_device_get_property_value(device, "SUBSYSTEM");
  if (!path || !subsystem)
    return nullptr;

  DeviceEvent::DeviceType device_type;
  if (!strcmp(subsystem, "input") &&
      base::StartsWith(path, "/dev/input/event", base::CompareCase::SENSITIVE))
    device_type = DeviceEvent::INPUT;
  else if (!strcmp(subsystem, "drm") &&
           base::StartsWith(path, "/dev/dri/card",
                            base::CompareCase::SENSITIVE))
    device_type = DeviceEvent::DISPLAY;
  else
    return nullptr;

  const char* action = device::udev_device_get_action(device);
  DeviceEvent::ActionType action_type;
  if (!action || !strcmp(action, "add"))
    action_type = DeviceEvent::ADD;
  else if (!strcmp(action, "remove"))
    action_type = DeviceEvent::REMOVE;
  else if (!strcmp(action, "change"))
    action_type = DeviceEvent::CHANGE;
  else
    return nullptr;

  PropertyMap property_map;
  udev_list_entry* property_list =
      device::udev_device_get_properties_list_entry(device);
  udev_list_entry* entry;
  udev_list_entry_foreach(entry, property_list) {
    const std::string key(device::udev_list_entry_get_name(entry));
    const std::string value(
        device::udev_device_get_property_value(device, key.c_str()));
    property_map.insert({key, value});
  }

  return std::make_unique<DeviceEvent>(device_type, action_type,
                                       base::FilePath(path), property_map);
}

}  // namespace ui
