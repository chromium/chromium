// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_output.h"

#include <xdg-output-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

XDGOutput::XDGOutput(zxdg_output_v1* xdg_output) : xdg_output_(xdg_output) {
  static const zxdg_output_v1_listener listener = {
      &XDGOutput::OutputHandleLogicalPosition,
      &XDGOutput::OutputHandleLogicalSize,
      &XDGOutput::OutputHandleDone,
      &XDGOutput::OutputHandleName,
      &XDGOutput::OutputHandleDescription,
  };
  // Can be nullptr in tests.
  if (xdg_output_)
    zxdg_output_v1_add_listener(xdg_output_.get(), &listener, this);
}

XDGOutput::~XDGOutput() = default;

// static
void XDGOutput::OutputHandleLogicalPosition(
    void* data,
    struct zxdg_output_v1* zxdg_output_v1,
    int32_t x,
    int32_t y) {
  if (XDGOutput* xdg_output = static_cast<XDGOutput*>(data))
    xdg_output->logical_position_ = gfx::Point(x, y);
}

// static
void XDGOutput::OutputHandleLogicalSize(void* data,
                                        struct zxdg_output_v1* zxdg_output_v1,
                                        int32_t width,
                                        int32_t height) {
  if (XDGOutput* xdg_output = static_cast<XDGOutput*>(data))
    xdg_output->logical_size_ = gfx::Size(width, height);
}

// static
void XDGOutput::OutputHandleDone(void* data,
                                 struct zxdg_output_v1* zxdg_output_v1) {
  // deprecated since version 3
}

bool XDGOutput::IsReady() const {
  return !logical_size_.IsEmpty();
}

// static
void XDGOutput::OutputHandleName(void* data,
                                 struct zxdg_output_v1* zxdg_output_v1,
                                 const char* name) {
  if (XDGOutput* xdg_output = static_cast<XDGOutput*>(data)) {
    xdg_output->name_ = name ? std::string(name) : std::string();
  }
}

// static
void XDGOutput::OutputHandleDescription(void* data,
                                        struct zxdg_output_v1* zxdg_output_v1,
                                        const char* description) {
  if (XDGOutput* xdg_output = static_cast<XDGOutput*>(data)) {
    xdg_output->description_ =
        description ? std::string(description) : std::string();
  }
}

}  // namespace ui
