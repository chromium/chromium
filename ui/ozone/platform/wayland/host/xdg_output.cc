// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_output.h"

#include <xdg-output-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_output.h"

namespace ui {

XDGOutput::XDGOutput(zxdg_output_v1* xdg_output) : xdg_output_(xdg_output) {
  // Can be nullptr in tests.
  if (xdg_output_) {
    static constexpr zxdg_output_v1_listener kXdgOutputListener = {
        .logical_position = &OnLogicalPosition,
        .logical_size = &OnLogicalSize,
        .done = &OnDone,
        .name = &OnName,
        .description = &OnDescription,
    };
    zxdg_output_v1_add_listener(xdg_output_.get(), &kXdgOutputListener, this);
  }
}

XDGOutput::~XDGOutput() = default;

bool XDGOutput::IsReady() const {
  return is_ready_;
}

void XDGOutput::HandleDone() {
  // If `logical_size` has been set the server must have propagated all the
  // necessary state events for this xdg_output.
  is_ready_ = !logical_size_.IsEmpty();
}

void XDGOutput::UpdateMetrics(bool surface_submission_in_pixel_coordinates,
                              WaylandOutput::Metrics& metrics) {
  if (!IsReady()) {
    return;
  }

  metrics.origin = logical_position_;
  metrics.logical_size = logical_size_;

  // Name is an optional xdg_output event.
  if (!name_.empty()) {
    metrics.name = name_;
  }

  // Description is an optional xdg_output event.
  if (!description_.empty()) {
    metrics.description = description_;
  }

  const gfx::Size logical_size = logical_size_;
  if (surface_submission_in_pixel_coordinates && !logical_size.IsEmpty()) {
    const gfx::Size physical_size = metrics.physical_size;
    DCHECK(!physical_size.IsEmpty());
    const float max_physical_side =
        std::max(physical_size.width(), physical_size.height());
    const float max_logical_side =
        std::max(logical_size.width(), logical_size.height());
    metrics.scale_factor = max_physical_side / max_logical_side;
  }
}

// static
void XDGOutput::OnLogicalPosition(void* data,
                                  zxdg_output_v1* output,
                                  int32_t x,
                                  int32_t y) {
  if (auto* self = static_cast<XDGOutput*>(data)) {
    self->logical_position_ = gfx::Point(x, y);
  }
}

// static
void XDGOutput::OnLogicalSize(void* data,
                              zxdg_output_v1* output,
                              int32_t width,
                              int32_t height) {
  if (auto* self = static_cast<XDGOutput*>(data)) {
    self->logical_size_ = gfx::Size(width, height);
  }
}

// static
void XDGOutput::OnDone(void* data, zxdg_output_v1* output) {
  // deprecated since version 3
}

// static
void XDGOutput::OnName(void* data, zxdg_output_v1* output, const char* name) {
  if (auto* self = static_cast<XDGOutput*>(data)) {
    self->name_ = name ? std::string(name) : std::string();
  }
}

// static
void XDGOutput::OnDescription(void* data,
                              zxdg_output_v1* output,
                              const char* description) {
  if (auto* self = static_cast<XDGOutput*>(data)) {
    self->description_ = description ? std::string(description) : std::string();
  }
}

}  // namespace ui
