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

int LibInputEventConverter::LibInputContext::Fd() {
  return libinput_get_fd(li_);
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

void LibInputEventConverter::OnFileCanReadWithoutBlocking(int fd) {}

}  // namespace ui
