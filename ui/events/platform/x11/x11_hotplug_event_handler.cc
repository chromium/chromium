// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_hotplug_event_handler.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/future.h"

namespace ui {

namespace {

enum DeviceType {
  DEVICE_TYPE_KEYBOARD,
  DEVICE_TYPE_MOUSE,
  DEVICE_TYPE_TOUCHPAD,
  DEVICE_TYPE_TOUCHSCREEN,
  DEVICE_TYPE_OTHER
};

using KeyboardDeviceCallback =
    base::OnceCallback<void(const std::vector<KeyboardDevice>&)>;

using TouchpadDeviceCallback =
    base::OnceCallback<void(const std::vector<TouchpadDevice>&)>;

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
  TouchpadDeviceCallback touchpad_callback;
  base::OnceClosure hotplug_finished_callback;
};

// Identical to FP3232_TO_DOUBLE from libxi's XExtInt.c
double Fp3232ToDouble(const x11::Input::Fp3232& x) {
  return static_cast<double>(x.integral) +
         static_cast<double>(x.frac) / (1ULL << 32);
}

// Stores a copy of the x11::Input::ValuatorClass values so X11 device
// processing can happen on a worker thread. This is needed since X11 structs
// are not copyable.
struct ValuatorClassInfo {
  explicit ValuatorClassInfo(const x11::Input::ValuatorClass& info)
      : label(static_cast<x11::Atom>(info.label)),
        max(Fp3232ToDouble(info.max)),
        min(Fp3232ToDouble(info.min)),
        mode(info.mode),
        number(info.number) {}

  x11::Atom label;
  double max;
  double min;
  x11::Input::ValuatorMode mode;
  uint16_t number;
};

// Stores a copy of the XITouchClassInfo values so X11 device processing can
// happen on a worker thread. This is needed since X11 structs are not copyable.
struct TouchClassInfo {
  TouchClassInfo() = default;

  explicit TouchClassInfo(const x11::Input::DeviceClass::Touch& info)
      : mode(info.mode), num_touches(info.num_touches) {}

  x11::Input::TouchMode mode{};
  int num_touches = 0;
};

struct DeviceInfo {
  DeviceInfo(const x11::Input::XIDeviceInfo& device,
             DeviceType type,
             const base::FilePath& path)
      : id(device.deviceid),
        name(device.name),
        use(device.type),
        type(type),
        path(path) {
    for (const auto& device_class : device.classes) {
      if (device_class.valuator.has_value()) {
        valuator_class_infos.emplace_back(*device_class.valuator);
      } else if (device_class.touch.has_value()) {
        // A device can have at most one XITouchClassInfo. Ref:
        // http://manpages.ubuntu.com/manpages/saucy/man3/XIQueryDevice.3.html
        CHECK(!touch_class_info.num_touches, base::NotFatalUntil::M163);
        touch_class_info = TouchClassInfo(*device_class.touch);
      }
    }
  }

  // Unique device identifier.
  x11::Input::DeviceId id;

  // Internal device name.
  std::string name;

  // Device type (ie: XIMasterPointer)
  x11::Input::DeviceType use;

  // Specifies the type of the device.
  DeviceType type;

  // Path to the actual device (ie: /dev/input/eventXX)
  base::FilePath path;

  std::vector<x11::Input::DeviceClass::Valuator> valuator_class_infos;

  TouchClassInfo touch_class_info;
};

// X11 display cache used on worker threads. This is filled on the UI thread and
// passed in to the worker threads.
struct DisplayState {
  x11::Atom mt_position_x;
  x11::Atom mt_position_y;
};

// Returns true if |name| is the name of a known invalid keyboard device. Note,
// this may return false negatives.
bool IsKnownInvalidKeyboardDevice(const std::string& name) {
  // TODO(https://crbug.com/41135719): Identify these devices using udev rules.
  constexpr auto kSet = base::MakeFixedFlatSet<std::string_view>(
      {"Power Button", "Sleep Button", "Video Bus", "gpio-keys.5",
       "gpio-keys.12", "ROCKCHIP-I2S Headset Jack"});

  std::string trimmed = name;
  base::TrimWhitespaceASCII(name, base::TRIM_TRAILING, &trimmed);

  return kSet.contains(trimmed);
}

// Returns true if |name| is the name of a known XTEST device. Note, this may
// return false negatives.
bool IsTestDevice(const std::string& name) {
  return name.find("XTEST") != std::string::npos;
}

// Helper used to parse keyboard information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleKeyboardDevicesInWorker(const std::vector<DeviceInfo>& device_infos,
                                   scoped_refptr<base::TaskRunner> reply_runner,
                                   KeyboardDeviceCallback callback) {
  std::vector<KeyboardDevice> devices;

  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_KEYBOARD)
      continue;
    if (device_info.use != x11::Input::DeviceType::SlaveKeyboard)
      continue;  // Assume all keyboards are keyboard slaves
    if (IsKnownInvalidKeyboardDevice(device_info.name))
      continue;  // Skip invalid devices.
    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    devices.emplace_back(static_cast<uint16_t>(device_info.id), type,
                         device_info.name);
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
  const DeviceInfo* virtual_pointer = nullptr;
  bool found_physical_pointer = false;

  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.use == x11::Input::DeviceType::SlavePointer ||
        device_info.use == x11::Input::DeviceType::FloatingSlave) {
      found_physical_pointer = true;
    }
    if (device_info.use == x11::Input::DeviceType::MasterPointer &&
        device_info.name == "Virtual core pointer") {
      virtual_pointer = &device_info;
    }
    if (device_info.type != DEVICE_TYPE_MOUSE ||
        device_info.use != x11::Input::DeviceType::SlavePointer) {
      continue;
    }

    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    devices.emplace_back(static_cast<uint16_t>(device_info.id), type,
                         device_info.name);
  }

  if (!found_physical_pointer && virtual_pointer) {
    InputDeviceType type = GetInputDeviceTypeFromPath(virtual_pointer->path);
    devices.emplace_back(static_cast<uint16_t>(virtual_pointer->id), type,
                         virtual_pointer->name);
  }

  reply_runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), devices));
}

// Helper used to parse touchpad information. When it is done it uses
// |reply_runner| and |callback| to update the state on the UI thread.
void HandleTouchpadDevicesInWorker(const std::vector<DeviceInfo>& device_infos,
                                   scoped_refptr<base::TaskRunner> reply_runner,
                                   TouchpadDeviceCallback callback) {
  std::vector<TouchpadDevice> devices;
  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_TOUCHPAD ||
        device_info.use != x11::Input::DeviceType::SlavePointer) {
      continue;
    }

    InputDeviceType type = GetInputDeviceTypeFromPath(device_info.path);
    devices.emplace_back(static_cast<uint16_t>(device_info.id), type,
                         device_info.name);
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
  if (display_state.mt_position_x == x11::Atom::None ||
      display_state.mt_position_y == x11::Atom::None)
    return;

  for (const DeviceInfo& device_info : device_infos) {
    if (device_info.type != DEVICE_TYPE_TOUCHSCREEN ||
        (device_info.use != x11::Input::DeviceType::FloatingSlave &&
         device_info.use != x11::Input::DeviceType::SlavePointer)) {
      continue;
    }

    // Touchscreens should be direct touch devices.
    if (device_info.touch_class_info.mode != x11::Input::TouchMode::Direct)
      continue;

    double max_x = -1.0;
    double max_y = -1.0;

    for (const auto& valuator : device_info.valuator_class_infos) {
      if (display_state.mt_position_x == valuator.label) {
        // Ignore X axis valuator with unexpected properties
        if (valuator.number == 0 &&
            valuator.mode == x11::Input::ValuatorMode::Absolute &&
            Fp3232ToDouble(valuator.min) == 0.0) {
          max_x = Fp3232ToDouble(valuator.max);
        }
      } else if (display_state.mt_position_y == valuator.label) {
        // Ignore Y axis valuator with unexpected properties
        if (valuator.number == 1 &&
            valuator.mode == x11::Input::ValuatorMode::Absolute &&
            Fp3232ToDouble(valuator.min) == 0.0) {
          max_y = Fp3232ToDouble(valuator.max);
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
      devices.emplace_back(static_cast<uint16_t>(device_info.id), type,
                           device_info.name, gfx::Size(max_x + 1, max_y + 1),
                           device_info.touch_class_info.num_touches,
                           has_stylus);
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

void OnKeyboardDevices(const std::vector<KeyboardDevice>& devices) {
  GetHotplugEventObserver()->OnKeyboardDevicesUpdated(devices);
}

void OnTouchscreenDevices(const std::vector<TouchscreenDevice>& devices) {
  GetHotplugEventObserver()->OnTouchscreenDevicesUpdated(devices);
}

void OnMouseDevices(const std::vector<InputDevice>& devices) {
  GetHotplugEventObserver()->OnMouseDevicesUpdated(devices);
}

void OnTouchpadDevices(const std::vector<TouchpadDevice>& devices) {
  GetHotplugEventObserver()->OnTouchpadDevicesUpdated(devices);
}

void OnHotplugFinished() {
  GetHotplugEventObserver()->OnDeviceListsComplete();
}

}  // namespace

struct X11HotplugEventHandler::PendingHotplug {
  PendingHotplug() = default;
  ~PendingHotplug() = default;

  uint64_t serial = 0;
  size_t pending_paths = 0;
  std::vector<DeviceInfo> device_infos;
  DisplayState display_state;
};

X11HotplugEventHandler::X11HotplugEventHandler() = default;

X11HotplugEventHandler::~X11HotplugEventHandler() = default;

void X11HotplugEventHandler::OnHotplugEvent() {
  auto* connection = x11::Connection::Get();
  const XDeviceList& device_list_xi =
      DeviceListCacheX11::GetInstance()->GetXDeviceList(connection);
  const XIDeviceList& device_list_xi2 =
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(connection);

  const int kMaxDeviceNum = 128;
  std::array<DeviceType, kMaxDeviceNum> device_types;
  for (auto& device_type : device_types)
    device_type = DEVICE_TYPE_OTHER;

  for (const auto& device : device_list_xi) {
    uint8_t id = device.device_id;
    if (id >= kMaxDeviceNum)
      continue;

    // In XWayland, physical devices are not exposed to X Server, but
    // rather X11 and Wayland uses wayland protocol to communicate
    // devices.

    // So, xinput that Chromium uses to enumerate devices prepends
    // "xwayland-" to each device name. Though, Wayland doesn't expose TOUCHPAD
    // directly. Instead, it's part of xwayland-pointer.
    x11::Atom type = device.device_type;
    if (type == connection->GetAtom("KEYBOARD") ||
        type == connection->GetAtom("xwayland-keyboard")) {
      device_types[id] = DEVICE_TYPE_KEYBOARD;
    } else if (type == connection->GetAtom("MOUSE") ||
               type == connection->GetAtom("xwayland-pointer")) {
      device_types[id] = DEVICE_TYPE_MOUSE;
    } else if (type == connection->GetAtom("TOUCHPAD")) {
      device_types[id] = DEVICE_TYPE_TOUCHPAD;
    } else if (type == connection->GetAtom("TOUCHSCREEN") ||
               type == connection->GetAtom("xwayland-touch")) {
      device_types[id] = DEVICE_TYPE_TOUCHSCREEN;
    }
  }

  auto state = std::make_unique<PendingHotplug>();
  const uint64_t serial = ++next_serial_;
  state->serial = serial;
  // X11 is not thread safe, so first get all the required state on the UI
  // thread.
  state->display_state.mt_position_x = connection->GetAtom("Abs MT Position X");
  state->display_state.mt_position_y = connection->GetAtom("Abs MT Position Y");

  // Input devices may carry a "Device Node" property pointing to their evdev
  // node, e.g. Device Node (250): "/dev/input/event8". Query it asynchronously
  // via XIGetProperty for non-master devices so hotplug handling never blocks
  // the UI thread on X server replies.
  const x11::Atom device_node = connection->GetAtom("Device Node");
  std::vector<std::pair<size_t, x11::Input::DeviceId>> path_queries;

  for (const auto& device : device_list_xi2) {
    if (!device.enabled || IsTestDevice(device.name))
      continue;

    auto deviceid = static_cast<uint16_t>(device.deviceid);
    DeviceType device_type =
        deviceid < kMaxDeviceNum ? device_types[deviceid] : DEVICE_TYPE_OTHER;
    const size_t index = state->device_infos.size();
    state->device_infos.emplace_back(device, device_type, base::FilePath());
    if (device_node != x11::Atom::None &&
        device.type != x11::Input::DeviceType::MasterPointer &&
        device.type != x11::Input::DeviceType::MasterKeyboard) {
      path_queries.emplace_back(index, device.deviceid);
    }
  }

  state->pending_paths = path_queries.size();
  pending_hotplug_ = std::move(state);

  if (path_queries.empty()) {
    FinishHotplugEvent();
    return;
  }

  std::vector<std::pair<size_t, x11::Future<x11::Input::XIGetPropertyReply>>>
      futures;
  futures.reserve(path_queries.size());
  for (const auto& [index, deviceid] : path_queries) {
    futures.emplace_back(index, connection->xinput().XIGetProperty({
                                    .deviceid = deviceid,
                                    .c_delete = false,
                                    .property = device_node,
                                    .type = x11::Atom::Any,
                                    .offset = 0,
                                    .len = std::numeric_limits<uint32_t>::max(),
                                }));
  }
  connection->Flush();

  for (auto& [index, future] : futures) {
    future.OnResponse(
        base::BindOnce(&X11HotplugEventHandler::OnDeviceNodeResponse,
                       weak_ptr_factory_.GetWeakPtr(), serial, index));
  }
}

void X11HotplugEventHandler::OnDeviceNodeResponse(
    uint64_t serial,
    size_t index,
    x11::Input::XIGetPropertyResponse response) {
  if (!pending_hotplug_ || pending_hotplug_->serial != serial) {
    return;
  }
  if (response && response->type == x11::Atom::STRING &&
      response->data8.has_value()) {
    pending_hotplug_->device_infos[index].path =
        base::FilePath(base::as_string_view(*response->data8));
  }
  CHECK_GT(pending_hotplug_->pending_paths, 0u);
  if (--pending_hotplug_->pending_paths == 0) {
    FinishHotplugEvent();
  }
}

void X11HotplugEventHandler::FinishHotplugEvent() {
  CHECK(pending_hotplug_);
  auto state = std::move(pending_hotplug_);

  UiCallbacks callbacks;
  callbacks.keyboard_callback = base::BindOnce(&OnKeyboardDevices);
  callbacks.touchscreen_callback = base::BindOnce(&OnTouchscreenDevices);
  callbacks.mouse_callback = base::BindOnce(&OnMouseDevices);
  callbacks.touchpad_callback = base::BindOnce(&OnTouchpadDevices);
  callbacks.hotplug_finished_callback = base::BindOnce(&OnHotplugFinished);

  // Parse the device information asynchronously since this operation may block.
  // Once the device information is extracted the parsed devices will be
  // returned via the callbacks.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&HandleHotplugEventInWorker,
                     std::move(state->device_infos), state->display_state,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     std::move(callbacks)));
}

}  // namespace ui
