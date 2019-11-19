// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_DEVICE_UDEV_DEVICE_MANAGER_UDEV_H_
#define UI_EVENTS_OZONE_DEVICE_UDEV_DEVICE_MANAGER_UDEV_H_

#include "base/macros.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/observer_list.h"
#include "device/udev_linux/scoped_udev.h"
#include "ui/events/ozone/device/device_manager.h"

namespace ui {

class DeviceEvent;
class DeviceEventObserver;

class DeviceManagerUdev : public DeviceManager,
                          base::MessagePumpForUI::FdWatcher {
 public:
  DeviceManagerUdev();
  ~DeviceManagerUdev() override;

 private:
  std::unique_ptr<DeviceEvent> ProcessMessage(udev_device* device);

  // Creates a device-monitor to look for device add/remove/change events.
  void CreateMonitor();

  // DeviceManager overrides:
  void ScanDevices(DeviceEventObserver* observer) override;
  void AddObserver(DeviceEventObserver* observer) override;
  void RemoveObserver(DeviceEventObserver* observer) override;

  // base::MessagePumpForUI::FdWatcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  device::ScopedUdevPtr udev_;
  device::ScopedUdevMonitorPtr monitor_;

  base::MessagePumpForUI::FdWatchController controller_;

  base::ObserverList<DeviceEventObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerUdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_DEVICE_UDEV_DEVICE_MANAGER_UDEV_H_
