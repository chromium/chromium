// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_hotplug_event_handler.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

#ifndef XI_PROP_PRODUCT_ID
#define XI_PROP_PRODUCT_ID "Device Product ID"
#endif

namespace ui {

namespace {

// Names of all known internal devices that should not be considered as
// keyboards.
// TODO(rsadam@): Identify these devices using udev rules. (Crbug.com/420728.)
const char* kKnownInvalidKeyboardDeviceNames[] = {"Power Button",
                                                  "Sleep Button",
                                                  "Video Bus",
                                                  "gpio-keys.5",
                                                  "gpio-keys.12",
                                                  "ROCKCHIP-I2S Headset Jack"};

enum DeviceType {
  DEVICE_TYPE_KEYBOARD,
  DEVICE_TYPE_MOUSE,
  DEVICE_TYPE_TOUCHPAD,
  DEVICE_TYPE_TOUCHSCREEN,
  DEVICE_TYPE_OTHER
};

using KeyboardDeviceCallback =
    base::OnceCallback<void(const std::vector<InputDevice>&)>;

using TouchscreenDeviceCallback =
    base::OnceCallback<void(const std::vector<TouchscreenDevice>&)>;

using InputDeviceCallback =
    base::OnceCallback<void(const std::vector<InputDevice>&)>;

// Used for updating the state on the UI thread once device information is
// parsed on helper threads.
struct UiCallbacks {
  KeyboardDeviceCallback keyboard_callback;
  TouchscreenDeviceCallback touchscreen_callback;
  InputDeviceCallback mouse_callback;
  InputDeviceCallback touchpad_callback;
  base::OnceClosure hotplug_finished_callback;
};

// Stores a copy of the XIValuatorClassInfo values so X11 device processing can
// happen on a worker thread. This is needed since X11 structs are not copyable.
struct ValuatorClassInfo {
  ValuatorClassInfo(const XIValuatorClassInfo& info)
      : label(info.label),
        max(info.max),
        min(info.min),
        mode(info.mode),
        number(info.number) {}

  Atom label;
  double max;
  double min;
  int mode;
  int number;
};

// Stores a copy of the XITouchClassInfo values so X11 device processing can
// happen on a worker thread. This is needed since X11 structs are not copyable.
struct TouchClassInfo {
  TouchClassInfo() : mode(0), num_touches(0) {}

  explicit TouchClassInfo(const XITouchClassInfo& info)
      : mode(info.mode), num_touches(info.num_touches) {}

  int mode;
  int num_touches;
};

struct DeviceInfo {
  DeviceInfo(const XIDeviceInfo& device,
             DeviceType type,
             const base::FilePath& path)
      : id(device.deviceid),
        name(device.name),
        use(device.use),
        type(type),
        path(path) {
    for (int i = 0; i < device.num_classes; ++i) {
      switch (device.classes[i]->type) {
        case XIValuatorClass:
          valuator_class_infos.push_back(ValuatorClassInfo(
              *reinterpret_cast<XIValuatorClassInfo*>(device.classes[i])));
          break;
        case XITouchClass:
          // A device can have at most one XITouchClassInfo. Ref:
          // http://manpages.ubuntu.com/manpages/saucy/man3/XIQueryDevice.3.html
          DCHECK(!touch_class_info.mode);
          touch_class_info = TouchClassInfo(
              *reinterpret_cast<XITouchClassInfo*>(device.classes[i]));
          break;
        default:
          break;
      }
    }
  }

  // Unique device identifier.
  int id;

  // Internal device name.
  std::string name;

  // Device type (ie: XIMasterPointer)
  int use;

  // Specifies the type of the device.
  DeviceType type;

  // Path to the actual device (ie: /dev/input/eventXX)
  base::FilePath path;

  std::vector<ValuatorClassInfo> valuator_class_infos;

  TouchClassInfo touch_class_info;
};

// X11 display cache used on worker threads. This is filled on the UI thread and
// passed in to the worker threads.
struct DisplayState {
  Atom mt_position_x;
  Atom mt_position_y;
};

// Returns true if |name| is the name of a known invalid keyboard device. Note,
// this may return false negatives.
bool IsKnownInvalidKeyboardDevice(const std::string& name) {
  std::string trimmed(name);
  base::TrimWhitespaceASCII(name, base::TRIM_TRAILING, &trimmed);
  for (const char* device_name : kKnownInvalidKeyboardDeviceNames) {
    if (trimmed == device_name)
      return true;
  }
  return false;
}

// Returns true if |name| is the name of a known XTEST device. Note, this may
// return false negatives.
bool IsTestDevice(const std::string& name) {
  return name.find("XTEST") != std::string::npos;
}

base::FilePath GetDevicePath(XDisplay* dpy, const XIDeviceInfo& device) {
  // Skip the main pointer and keyboard since XOpenDevice() generates a
  // BadDevice error when passed these devices.
  if (device.use == XIMasterPointer || device.use == XIMasterKeyboard)
    return base::FilePath();

  // Input device has a property "Device Node" pointing to its dev input node,
  // e.g.   Device Node (250): "/dev/input/event8"
  Atom device_node = gfx::GetAtom("Device Node");
  if (device_node == x11::None)
    return base::FilePath();

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char* data;
  XDevice* dev = XOpenDevice(dpy, device.deviceid);

  // Sometimes XOpenDevice() doesn't return null but the contents aren't valid.
  // Calling XGetDeviceProperty() when dev->device_id is invalid triggers a
  // BadDevice error. Return early to avoid a crash. http://crbug.com/659261
  if (!dev || dev->device_id != base::checked_cast<XID>(device.deviceid))
    return base::FilePath();

  if (XGetDeviceProperty(dpy, dev, device_node, 0, 1000, x11::False,
                         AnyPropertyType, &actual_type, &actual_format, &nitems,
                         &bytes_after, &data) != x11::Success) {
    XCloseDevice(dpy, dev);
    return base::FilePath();
  }

  std::string path;
  // Make sure the returned value is a string.
  if (actual_type == XA_STRING && actual_format == 8)
    path = reinterpret_cast<char*>(data);

  XFree(data);
  XCloseDevice(dpy, dev);

  return base::FilePath(path);
}

// Helper used to parse keyboard information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleKeyboardDevicesInWorker(const std::vector<DeviceInfo>& device_infos,
                                   scoped_refptr<base::TaskRunner> reply_runner,
                                   KeyboardDeviceCallback callback) {
  std::vector<InputDevice> devices;

  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_KEYBOARD)
      continue;
    if (device_info.use != XISlaveKeyboard)
      continue;  // Assume all keyboards are keyboard slaves
    if (IsKnownInvalidKeyboardDevice(device_info.name))
      continue;  // Skip invalid devices.
    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    InputDevice keyboard(device_info.id, type, device_info.name);
    devices.push_back(keyboard);
  }

  reply_runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), devices));
}

// Helper used to parse mouse information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleMouseDevicesInWorker(const std::vector<DeviceInfo>& device_infos,
                                scoped_refptr<base::TaskRunner> reply_runner,
                                InputDeviceCallback callback) {
  std::vector<InputDevice> devices;
  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_MOUSE ||
        device_info.use != XISlavePointer) {
      continue;
    }

    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    devices.push_back(InputDevice(device_info.id, type, device_info.name));
  }

  reply_runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), devices));
}

// Helper used to parse touchpad information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleTouchpadDevicesInWorker(const std::vector<DeviceInfo>& device_infos,
                                   scoped_refptr<base::TaskRunner> reply_runner,
                                   InputDeviceCallback callback) {
  std::vector<InputDevice> devices;
  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_TOUCHPAD ||
        device_info.use != XISlavePointer) {
      continue;
    }

    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    devices.push_back(InputDevice(device_info.id, type, device_info.name));
  }

  reply_runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), devices));
}

// Helper used to parse touchscreen information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleTouchscreenDevicesInWorker(
    const std::vector<DeviceInfo>& device_infos,
    const DisplayState& display_state,
    scoped_refptr<base::TaskRunner> reply_runner,
    TouchscreenDeviceCallback callback) {
  std::vector<TouchscreenDevice> devices;
  if (display_state.mt_position_x == x11::None ||
      display_state.mt_position_y == x11::None)
    return;

  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_TOUCHSCREEN ||
        (device_info.use != XIFloatingSlave &&
         device_info.use != XISlavePointer)) {
      continue;
    }

    // Touchscreens should be direct touch devices.
    if (device_info.touch_class_info.mode != XIDirectTouch)
      continue;

    double max_x = -1.0;
    double max_y = -1.0;

    for (const ValuatorClassInfo& valuator : device_info.valuator_class_infos) {
      if (display_state.mt_position_x == valuator.label) {
        // Ignore X axis valuator with unexpected properties
        if (valuator.number == 0 && valuator.mode == Absolute &&
            valuator.min == 0.0) {
          max_x = valuator.max;
        }
      } else if (display_state.mt_position_y == valuator.label) {
        // Ignore Y axis valuator with unexpected properties
        if (valuator.number == 1 && valuator.mode == Absolute &&
            valuator.min == 0.0) {
          max_y = valuator.max;
        }
      }
    }

    // Touchscreens should have absolute X and Y axes.
    if (max_x > 0.0 && max_y > 0.0) {
      InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
      // TODO(jamescook): Detect pen/stylus.
      const bool has_stylus = false;
      // |max_x| and |max_y| are inclusive values, so we need to add 1 to get
      // the size.
      devices.push_back(TouchscreenDevice(
          device_info.id, type, device_info.name,
          gfx::Size(max_x + 1, max_y + 1),
          device_info.touch_class_info.num_touches, has_stylus));
    }
  }

  reply_runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), devices));
}

// Called on a worker thread to parse the device information.
void HandleHotplugEventInWorker(const std::vector<DeviceInfo>& devices,
                                const DisplayState& display_state,
                                scoped_refptr<base::TaskRunner> reply_runner,
                                UiCallbacks callbacks) {
  HandleTouchscreenDevicesInWorker(devices, display_state, reply_runner,
                                   std::move(callbacks.touchscreen_callback));
  HandleKeyboardDevicesInWorker(devices, reply_runner,
                                std::move(callbacks.keyboard_callback));
  HandleMouseDevicesInWorker(devices, reply_runner,
                             std::move(callbacks.mouse_callback));
  HandleTouchpadDevicesInWorker(devices, reply_runner,
                                std::move(callbacks.touchpad_callback));
  reply_runner->PostTask(FROM_HERE,
                         std::move(callbacks.hotplug_finished_callback));
}

DeviceHotplugEventObserver* GetHotplugEventObserver() {
  return DeviceDataManager::GetInstance();
}

void OnKeyboardDevices(const std::vector<InputDevice>& devices) {
  GetHotplugEventObserver()->OnKeyboardDevicesUpdated(devices);
}

void OnTouchscreenDevices(const std::vector<TouchscreenDevice>& devices) {
  GetHotplugEventObserver()->OnTouchscreenDevicesUpdated(devices);
}

void OnMouseDevices(const std::vector<InputDevice>& devices) {
  GetHotplugEventObserver()->OnMouseDevicesUpdated(devices);
}

void OnTouchpadDevices(const std::vector<InputDevice>& devices) {
  GetHotplugEventObserver()->OnTouchpadDevicesUpdated(devices);
}

void OnHotplugFinished() {
  GetHotplugEventObserver()->OnDeviceListsComplete();
}

}  // namespace

X11HotplugEventHandler::X11HotplugEventHandler() {}

X11HotplugEventHandler::~X11HotplugEventHandler() {
}

void X11HotplugEventHandler::OnHotplugEvent() {
  Display* display = gfx::GetXDisplay();
  const XDeviceList& device_list_xi =
      DeviceListCacheX11::GetInstance()->GetXDeviceList(display);
  const XIDeviceList& device_list_xi2 =
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(display);

  const int kMaxDeviceNum = 128;
  DeviceType device_types[kMaxDeviceNum];
  for (int i = 0; i < kMaxDeviceNum; ++i)
    device_types[i] = DEVICE_TYPE_OTHER;

  for (int i = 0; i < device_list_xi.count; ++i) {
    int id = device_list_xi[i].id;
    if (id < 0 || id >= kMaxDeviceNum)
      continue;

    Atom type = device_list_xi[i].type;
    if (type == gfx::GetAtom(XI_KEYBOARD))
      device_types[id] = DEVICE_TYPE_KEYBOARD;
    else if (type == gfx::GetAtom(XI_MOUSE))
      device_types[id] = DEVICE_TYPE_MOUSE;
    else if (type == gfx::GetAtom(XI_TOUCHPAD))
      device_types[id] = DEVICE_TYPE_TOUCHPAD;
    else if (type == gfx::GetAtom(XI_TOUCHSCREEN))
      device_types[id] = DEVICE_TYPE_TOUCHSCREEN;
  }

  std::vector<DeviceInfo> device_infos;
  for (int i = 0; i < device_list_xi2.count; ++i) {
    const XIDeviceInfo& device = device_list_xi2[i];
    if (!device.enabled || IsTestDevice(device.name))
      continue;

    DeviceType device_type =
        (device.deviceid >= 0 && device.deviceid < kMaxDeviceNum)
            ? device_types[device.deviceid]
            : DEVICE_TYPE_OTHER;
    device_infos.push_back(
        DeviceInfo(device, device_type, GetDevicePath(display, device)));
  }

  // X11 is not thread safe, so first get all the required state.
  DisplayState display_state;
  display_state.mt_position_x = gfx::GetAtom("Abs MT Position X");
  display_state.mt_position_y = gfx::GetAtom("Abs MT Position Y");

  UiCallbacks callbacks;
  callbacks.keyboard_callback = base::BindOnce(&OnKeyboardDevices);
  callbacks.touchscreen_callback = base::BindOnce(&OnTouchscreenDevices);
  callbacks.mouse_callback = base::BindOnce(&OnMouseDevices);
  callbacks.touchpad_callback = base::BindOnce(&OnTouchpadDevices);
  callbacks.hotplug_finished_callback = base::BindOnce(&OnHotplugFinished);

  // Parse the device information asynchronously since this operation may block.
  // Once the device information is extracted the parsed devices will be
  // returned via the callbacks.
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HandleHotplugEventInWorker, device_infos, display_state,
                     base::ThreadTaskRunnerHandle::Get(),
                     std::move(callbacks)));
}

}  // namespace ui
