// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output.h"

#include <wayland-client.h>

#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandOutput::WaylandOutput(const uint32_t output_id, wl_output* output)
    : output_id_(output_id),
      output_(output),
      scale_factor_(kDefaultScaleFactor),
      rect_in_physical_pixels_(gfx::Rect()) {}

WaylandOutput::~WaylandOutput() = default;

void WaylandOutput::Initialize(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  static const wl_output_listener output_listener = {
      &WaylandOutput::OutputHandleGeometry,
      &WaylandOutput::OutputHandleMode,
      &WaylandOutput::OutputHandleDone,
      &WaylandOutput::OutputHandleScale,
  };
  wl_output_add_listener(output_.get(), &output_listener, this);
}

void WaylandOutput::TriggerDelegateNotification() const {
  DCHECK(!rect_in_physical_pixels_.IsEmpty());
  delegate_->OnOutputHandleMetrics(output_id_, rect_in_physical_pixels_,
                                   scale_factor_);
}

// static
void WaylandOutput::OutputHandleGeometry(void* data,
                                         wl_output* output,
                                         int32_t x,
                                         int32_t y,
                                         int32_t physical_width,
                                         int32_t physical_height,
                                         int32_t subpixel,
                                         const char* make,
                                         const char* model,
                                         int32_t output_transform) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output)
    wayland_output->rect_in_physical_pixels_.set_origin(gfx::Point(x, y));
}

// static
void WaylandOutput::OutputHandleMode(void* data,
                                     wl_output* wl_output,
                                     uint32_t flags,
                                     int32_t width,
                                     int32_t height,
                                     int32_t refresh) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output && (flags & WL_OUTPUT_MODE_CURRENT))
    wayland_output->rect_in_physical_pixels_.set_size(gfx::Size(width, height));
}

// static
void WaylandOutput::OutputHandleDone(void* data, struct wl_output* wl_output) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output)
    wayland_output->TriggerDelegateNotification();
}

// static
void WaylandOutput::OutputHandleScale(void* data,
                                      struct wl_output* wl_output,
                                      int32_t factor) {
  WaylandOutput* wayland_output = static_cast<WaylandOutput*>(data);
  if (wayland_output)
    wayland_output->scale_factor_ = factor;
}

}  // namespace ui
