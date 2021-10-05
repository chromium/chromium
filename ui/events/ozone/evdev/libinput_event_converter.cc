// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libinput_event_converter.h"

#include <fcntl.h>

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

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

libinput_event_type LibInputEventConverter::LibInputEvent::Type() const {
  return libinput_event_get_type(event_);
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

absl::optional<LibInputEventConverter::LibInputContext>
LibInputEventConverter::LibInputContext::Create() {
  libinput* const li = libinput_path_create_context(&interface_, nullptr);
  if (!li) {
    LOG(ERROR) << "libinput_path_create_context failed";
    return absl::nullopt;
  }

  return absl::make_optional(LibInputEventConverter::LibInputContext(li));
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

absl::optional<LibInputEventConverter::LibInputEvent>
LibInputEventConverter::LibInputContext::NextEvent() const {
  libinput_event* const event = libinput_get_event(li_);
  if (!event) {
    return absl::nullopt;
  }
  return absl::make_optional(LibInputEvent(event));
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
    const EventDeviceInfo& devinfo) {
  auto context = LibInputContext::Create();
  if (!context) {
    LOG(ERROR) << "LibInputContext::Create failed";
    return nullptr;
  }

  return std::make_unique<LibInputEventConverter>(std::move(context.value()),
                                                  path, id, devinfo);
}

LibInputEventConverter::LibInputEventConverter(
    LibInputEventConverter::LibInputContext&& ctx,
    const base::FilePath& path,
    int id,
    const EventDeviceInfo& devinfo)
    : EventConverterEvdev(ctx.Fd(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      has_keyboard_(devinfo.HasKeyboard()),
      has_mouse_(devinfo.HasMouse()),
      has_touchpad_(devinfo.HasTouchpad()),
      has_touchscreen_(devinfo.HasTouchscreen()),
      context_(std::move(ctx)) {}

LibInputEventConverter::~LibInputEventConverter() {}

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
    case LIBINPUT_EVENT_POINTER_BUTTON:
    case LIBINPUT_EVENT_POINTER_AXIS:
    case LIBINPUT_EVENT_TOUCH_DOWN:
    case LIBINPUT_EVENT_TOUCH_UP:
    case LIBINPUT_EVENT_TOUCH_MOTION:
    case LIBINPUT_EVENT_TOUCH_CANCEL:
    case LIBINPUT_EVENT_TOUCH_FRAME:
    case LIBINPUT_EVENT_NONE:
    case LIBINPUT_EVENT_DEVICE_ADDED:
    case LIBINPUT_EVENT_DEVICE_REMOVED:
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
    case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
    case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
    case LIBINPUT_EVENT_TABLET_TOOL_TIP:
    case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
    case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
    case LIBINPUT_EVENT_TABLET_PAD_RING:
    case LIBINPUT_EVENT_TABLET_PAD_STRIP:
    case LIBINPUT_EVENT_KEYBOARD_KEY:
    case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
    case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
    case LIBINPUT_EVENT_GESTURE_SWIPE_END:
    case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
    case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
    case LIBINPUT_EVENT_GESTURE_PINCH_END:
    case LIBINPUT_EVENT_SWITCH_TOGGLE:
    case LIBINPUT_EVENT_TABLET_PAD_KEY:
      DVLOG(3) << "Ignoring libinput event: " << event.Type();
      break;
  }
}

}  // namespace ui
