// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_util_linux.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"

namespace ui {

base::FilePath GetInputPathInSys(const base::FilePath& path) {
  return base::MakeAbsoluteFilePath(
      base::FilePath("/sys/class/input").Append(path.BaseName()));
}

InputDeviceType GetInputDeviceTypeFromPath(const base::FilePath& path) {
  std::string event_node = path.BaseName().value();
  if (event_node.empty() ||
      !base::StartsWith(event_node, "event", base::CompareCase::SENSITIVE))
    return InputDeviceType::INPUT_DEVICE_UNKNOWN;

  base::FilePath sysfs_path = GetInputPathInSys(path);

  // Device does not exist.
  if (sysfs_path.empty())
    return InputDeviceType::INPUT_DEVICE_UNKNOWN;

  // Check ancestor devices for a known bus attachment.
  for (base::FilePath ancestor_path = sysfs_path;
       ancestor_path != base::FilePath("/");
       ancestor_path = ancestor_path.DirName()) {
    // Bluetooth LE devices are virtual "uhid" devices.
    if (ancestor_path ==
        base::FilePath(FILE_PATH_LITERAL("/sys/devices/virtual/misc/uhid"))) {
      return InputDeviceType::INPUT_DEVICE_BLUETOOTH;
    }

    std::string subsystem_path =
        base::MakeAbsoluteFilePath(
            ancestor_path.Append(FILE_PATH_LITERAL("subsystem")))
            .value();
    if (subsystem_path.empty())
      continue;

    // Internal bus attachments.
    if (subsystem_path == "/sys/bus/pci")
      return InputDeviceType::INPUT_DEVICE_INTERNAL;
    if (subsystem_path == "/sys/bus/i2c")
      return InputDeviceType::INPUT_DEVICE_INTERNAL;
    if (subsystem_path == "/sys/bus/acpi")
      return InputDeviceType::INPUT_DEVICE_INTERNAL;
    if (subsystem_path == "/sys/bus/serio")
      return InputDeviceType::INPUT_DEVICE_INTERNAL;
    if (subsystem_path == "/sys/bus/platform")
      return InputDeviceType::INPUT_DEVICE_INTERNAL;

    // External bus attachments.
    if (subsystem_path == "/sys/bus/usb")
      return InputDeviceType::INPUT_DEVICE_USB;
    if (subsystem_path == "/sys/class/bluetooth")
      return InputDeviceType::INPUT_DEVICE_BLUETOOTH;
  }

  return InputDeviceType::INPUT_DEVICE_UNKNOWN;
}

}  // namespace ui
