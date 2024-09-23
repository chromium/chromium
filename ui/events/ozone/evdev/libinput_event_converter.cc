// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libinput_event_converter.h"

#include <fcntl.h>

#include <ostream>
#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

namespace ui {

namespace {

double GetAxisValue(libinput_event_pointer* const event,
                    const libinput_pointer_axis axis) {
  // Check has_axis before get_axis_value to avoid spamming the logs
  // with libinput errors
  if (!libinput_event_pointer_has_axis(event, axis)) {
    return 0.0;
  }

  // Libinput's scroll axes are reversed compared to Chromium. For
  // example, libinput produces positive deltas when scrolling down
  // (with natural scrolling off), but Chromium's event system
  // produces negative deltas. Simple fix: negate the axis value.
  return -libinput_event_pointer_get_axis_value(event, axis);
}

// Log the result of setting a device configuration value.
template <typename Value>
void LogConfigStatus(const libinput_config_status status,
                     const char* key,
                     const Value value) {
  switch (status) {
    case LIBINPUT_CONFIG_STATUS_SUCCESS:
      DVLOG(3) << "libinput config: set " << key << " to " << value;
      break;

    case LIBINPUT_CONFIG_STATUS_UNSUPPORTED:
      LOG(ERROR) << "libinput config: " << key << " not supported";
      break;

    case LIBINPUT_CONFIG_STATUS_INVALID:
      LOG(ERROR) << "libinput config: invalid parameter for " << key << ": "
                 << value;
      break;
  }
}

}  // namespace

LibInputEventConverter::LibInputEvent::LibInputEvent(LibInputEvent&& other)
    : event_(other.event_) {
  other.event_ = nullptr;
}

LibInputEventConverter::LibInputEvent::LibInputEvent(
    libinput_event* const event)
    : event_(event) {}

LibInputEventConverter::LibInputEvent::~LibInputEvent() {
  if (event_) {
    libinput_event_destroy(event_);
  }
}

libinput_event_pointer* LibInputEventConverter::LibInputEvent::PointerEvent()
    const {
  auto* const event = libinput_event_get_pointer_event(event_);
  DCHECK(event);
  return event;
}

libinput_event_type LibInputEventConverter::LibInputEvent::Type() const {
  return libinput_event_get_type(event_);
}

LibInputEventConverter::LibInputDevice::LibInputDevice(
    int id,
    libinput_device* const device)
    : device_id_(id), device_(device) {
  DCHECK(device);

  // |libinput_path_add_device| returns a device pointer that is is
  // derefed at the next call to |libinput_dispatch|. Add a ref here
  // so that we can keep using the pointer.
  libinput_device_ref(device);

  DVLOG(2) << "device: \"" << libinput_device_get_name(device_)
           << "\", capabilities: " << GetCapabilitiesString();
}

LibInputEventConverter::LibInputDevice::LibInputDevice(LibInputDevice&& other)
    : device_id_(other.device_id_), device_(other.device_) {
  other.device_ = nullptr;
}

LibInputEventConverter::LibInputDevice::~LibInputDevice() {
  if (device_) {
    libinput_device_unref(device_);
  }
}

void LibInputEventConverter::LibInputDevice::ApplySettings(
    const InputDeviceSettingsEvdev& settings) const {
  const auto& touchpad_settings = settings.GetTouchpadSettings(device_id_);
  SetNaturalScrollEnabled(touchpad_settings.natural_scroll_enabled);
  SetSensitivity(touchpad_settings.sensitivity);
  SetTapToClickEnabled(touchpad_settings.tap_to_click_enabled);
}

// Get a comma-separated string of the device's capabilities
std::string LibInputEventConverter::LibInputDevice::GetCapabilitiesString() {
  // All capabilities exposed by libinput
  std::vector<std::pair<libinput_device_capability, std::string>> caps = {
      {LIBINPUT_DEVICE_CAP_KEYBOARD, "keyboard"},
      {LIBINPUT_DEVICE_CAP_POINTER, "pointer"},
      {LIBINPUT_DEVICE_CAP_TOUCH, "touch"},
      {LIBINPUT_DEVICE_CAP_TABLET_TOOL, "tablet-tool"},
      {LIBINPUT_DEVICE_CAP_TABLET_PAD, "tablet-pad"},
      {LIBINPUT_DEVICE_CAP_GESTURE, "gesture"},
      {LIBINPUT_DEVICE_CAP_SWITCH, "switch"}};

  std::vector<std::string> parts(caps.size());
  for (const auto& pair : caps) {
    if (libinput_device_has_capability(device_, pair.first)) {
      parts.emplace_back(pair.second);
    }
  }

  return base::JoinString(parts, ", ");
}

void LibInputEventConverter::LibInputDevice::SetNaturalScrollEnabled(
    const bool enabled) const {
  const auto status = libinput_device_config_scroll_set_natural_scroll_enabled(
      device_, enabled);
  LogConfigStatus(status, "natural-scroll", enabled);
}

void LibInputEventConverter::LibInputDevice::SetSensitivity(
    const int sensitivity) const {
  // The range of |sensitivity| is [1..5] according to comments in
  // libgestures. Rescale to floating point [-1, 1].
  const double speed = (sensitivity - 3.0) / 2.0;
  const auto status = libinput_device_config_accel_set_speed(device_, speed);
  LogConfigStatus(status, "sensitivity", speed);
}

void LibInputEventConverter::LibInputDevice::SetTapToClickEnabled(
    const bool enabled) const {
  const auto arg =
      (enabled ? LIBINPUT_CONFIG_TAP_ENABLED : LIBINPUT_CONFIG_TAP_DISABLED);

  const auto status = libinput_device_config_tap_set_enabled(device_, arg);
  LogConfigStatus(status, "tap-to-click", arg);
}

LibInputEventConverter::LibInputContext::LibInputContext(
    LibInputContext&& other)
    : li_(other.li_) {
  other.li_ = nullptr;
}

LibInputEventConverter::LibInputContext::LibInputContext(libinput* const li)
    : li_(li) {
  DCHECK(li_);
  libinput_log_set_handler(li_, LogHandler);
  libinput_log_set_priority(li_, LIBINPUT_LOG_PRIORITY_DEBUG);
}

LibInputEventConverter::LibInputContext::~LibInputContext() {
  if (li_) {
    libinput_unref(li_);
  }
}

std::optional<LibInputEventConverter::LibInputContext>
LibInputEventConverter::LibInputContext::Create() {
  libinput* const li = libinput_path_create_context(&interface_, nullptr);
  if (!li) {
    LOG(ERROR) << "libinput_path_create_context failed";
    return std::nullopt;
  }

  return std::make_optional(LibInputEventConverter::LibInputContext(li));
}

std::optional<LibInputEventConverter::LibInputDevice>
LibInputEventConverter::LibInputContext::AddDevice(
    int id,
    const base::FilePath& path) const {
  auto* const dev = libinput_path_add_device(li_, path.value().c_str());
  if (!dev) {
    LOG(ERROR) << "libinput_path_add_device failed with device: " << path;
    return std::nullopt;
  }

  return std::make_optional(LibInputDevice(id, dev));
}

bool LibInputEventConverter::LibInputContext::Dispatch() const {
  const auto rc = libinput_dispatch(li_);
  if (rc != 0) {
    LOG(ERROR) << "libinput_dispatch failed: " << rc;
    return false;
  }
  return true;
}

int LibInputEventConverter::LibInputContext::Fd() {
  return libinput_get_fd(li_);
}

std::optional<LibInputEventConverter::LibInputEvent>
LibInputEventConverter::LibInputContext::NextEvent() const {
  libinput_event* const event = libinput_get_event(li_);
  if (!event) {
    return std::nullopt;
  }
  return std::make_optional(LibInputEvent(event));
}

int LibInputEventConverter::LibInputContext::OpenRestricted(const char* path,
                                                            int flags,
                                                            void* user_data) {
  VLOG(1) << "Open input: " << path;
  int fd = open(path, flags);
  return fd;
}

void LibInputEventConverter::LibInputContext::CloseRestricted(int fd,
                                                              void* user_data) {
  close(fd);
}

void LibInputEventConverter::LibInputContext::LogHandler(
    libinput* libinput,
    enum libinput_log_priority priority,
    const char* format,
    va_list args) {
  switch (priority) {
    case LIBINPUT_LOG_PRIORITY_DEBUG:
      VLOG(4) << "libinput: " << base::StringPrintV(format, args);
      break;

    case LIBINPUT_LOG_PRIORITY_INFO:
      VLOG(1) << "libinput: " << base::StringPrintV(format, args);
      break;

    case LIBINPUT_LOG_PRIORITY_ERROR:
      LOG(ERROR) << "libinput: " << base::StringPrintV(format, args);
      break;
  }
}

constexpr libinput_interface
    LibInputEventConverter::LibInputContext::interface_;

std::unique_ptr<LibInputEventConverter> LibInputEventConverter::Create(
    const base::FilePath& path,
    int id,
    const EventDeviceInfo& devinfo,
    CursorDelegateEvdev* cursor,
    DeviceEventDispatcherEvdev* dispatcher) {
  auto context = LibInputContext::Create();
  if (!context) {
    LOG(ERROR) << "LibInputContext::Create failed";
    return nullptr;
  }

  return std::make_unique<LibInputEventConverter>(
      std::move(context.value()), path, id, devinfo, cursor, dispatcher);
}

LibInputEventConverter::LibInputEventConverter(
    LibInputEventConverter::LibInputContext&& ctx,
    const base::FilePath& path,
    int id,
    const EventDeviceInfo& devinfo,
    CursorDelegateEvdev* cursor,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(ctx.Fd(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      dispatcher_(dispatcher),
      cursor_(cursor),
      has_keyboard_(devinfo.HasKeyboard()),
      has_mouse_(devinfo.HasMouse()),
      has_touchpad_(devinfo.HasTouchpad()),
      has_touchscreen_(devinfo.HasTouchscreen()),
      context_(std::move(ctx)),
      device_(context_.AddDevice(id, path)) {}

LibInputEventConverter::~LibInputEventConverter() {}

void LibInputEventConverter::ApplyDeviceSettings(
    const InputDeviceSettingsEvdev& settings) {
  if (device_) {
    device_->ApplySettings(settings);
  } else {
    LOG(ERROR)
        << "Unable to apply settings due to libinput_path_add_device failure";
  }
}

bool LibInputEventConverter::HasKeyboard() const {
  return has_keyboard_;
}

bool LibInputEventConverter::HasMouse() const {
  return has_mouse_;
}

bool LibInputEventConverter::HasTouchpad() const {
  return has_touchpad_;
}

bool LibInputEventConverter::HasTouchscreen() const {
  return has_touchscreen_;
}

void LibInputEventConverter::OnFileCanReadWithoutBlocking(int fd) {
  if (!context_.Dispatch()) {
    LOG(ERROR) << "LibInputContext::Dispatch failed";
    return;
  }

  while (auto event = context_.NextEvent()) {
    HandleEvent(*event);
  }
}

void LibInputEventConverter::HandleEvent(const LibInputEvent& event) {
  switch (event.Type()) {
    case LIBINPUT_EVENT_POINTER_MOTION:
      HandlePointerMotion(event);
      break;

    case LIBINPUT_EVENT_POINTER_BUTTON:
      HandlePointerButton(event);
      break;

    case LIBINPUT_EVENT_POINTER_AXIS:
      HandlePointerAxis(event);
      break;

    default:
      DVLOG(3) << "Ignoring libinput event: " << event.Type();
      break;
  }
}

void LibInputEventConverter::HandlePointerMotion(const LibInputEvent& evt) {
  libinput_event_pointer* event = evt.PointerEvent();
  const int flags = EF_NONE;
  const double dx = libinput_event_pointer_get_dx(event);
  const double dy = libinput_event_pointer_get_dy(event);

  cursor_->MoveCursor(gfx::Vector2dF(dx, dy));

  DVLOG(3) << "Pointer motion: dx=" << dx << ", dy=" << dy;

  dispatcher_->DispatchMouseMoveEvent(
      {input_device_.id, flags, cursor_->GetLocation(), nullptr /*delta*/,
       PointerDetails(EventPointerType::kMouse), Timestamp(evt)});
}

void LibInputEventConverter::HandlePointerButton(const LibInputEvent& evt) {
  libinput_event_pointer* event = evt.PointerEvent();
  const int flags = EF_NONE;
  const uint32_t button = libinput_event_pointer_get_button(event);
  const bool down = libinput_event_pointer_get_button_state(event) ==
                    LIBINPUT_BUTTON_STATE_PRESSED;

  // allow_remap: Controls whether or not remapping buttons is allowed; in
  // this case whether or not it should respect the "Swap Left/Right Mouse
  // Button" option in the ChromeOS settings.
  //
  // Since we only deal with touchpad buttons here, this should be false,
  // but if we expand this to handle mice in the future we may need to
  // have more intricate logic.
  const MouseButtonMapType allow_remap = MouseButtonMapType::kNone;

  DVLOG(3) << "Button: " << button << ", pressed: " << down;

  dispatcher_->DispatchMouseButtonEvent(
      {input_device_.id, flags, cursor_->GetLocation(), button, down,
       allow_remap, PointerDetails(EventPointerType::kMouse), Timestamp(evt)});
}

void LibInputEventConverter::HandlePointerAxis(const LibInputEvent& evt) {
  libinput_event_pointer* event = evt.PointerEvent();
  const auto h = GetAxisValue(event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
  const auto v = GetAxisValue(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
  const gfx::Vector2d delta(h, v);

  DVLOG(3) << "Pointer axis h:" << h << ", v:" << v;

  dispatcher_->DispatchScrollEvent({input_device_.id, EventType::kScroll,
                                    cursor_->GetLocation(), delta, delta, 2,
                                    Timestamp(evt)});
}

base::TimeTicks LibInputEventConverter::Timestamp(const LibInputEvent& evt) {
  libinput_event_pointer* event = evt.PointerEvent();
  uint64_t time_usec = libinput_event_pointer_get_time_usec(event);
  return base::TimeTicks() + base::Microseconds(time_usec);
}

std::ostream& LibInputEventConverter::DescribeForLog(std::ostream& os) const {
  os << "class=ui::LibInputEventConverter id=" << input_device_.id << std::endl
     << " has_keyboard=" << has_keyboard_ << std::endl
     << " has_mouse=" << has_mouse_ << std::endl
     << " has_touchpad=" << has_touchpad_ << std::endl
     << " has_touchscreen=" << has_touchscreen_ << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

}  // namespace ui
