// Copyright 2021 The Chromium Authors. All rights reserved.
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
  zxdg_output_v1_add_listener(xdg_output_.get(), &listener, this);
}

XDGOutput::~XDGOutput() = default;

// static
void XDGOutput::OutputHandleLogicalPosition(
    void* data,
    struct zxdg_output_v1* zxdg_output_v1,
    int32_t x,
    int32_t y) {}

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

// static
void XDGOutput::OutputHandleName(void* data,
                                 struct zxdg_output_v1* zxdg_output_v1,
                                 const char* name) {}

// static
void XDGOutput::OutputHandleDescription(void* data,
                                        struct zxdg_output_v1* zxdg_output_v1,
                                        const char* description) {}

}  // namespace ui
