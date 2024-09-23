// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_output.h"

#include <xdg-output-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/common/wayland_util.h"
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

void XDGOutput::UpdateMetrics(bool compute_scale_from_size,
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
  const gfx::Size physical_size = metrics.physical_size;
  DCHECK(!physical_size.IsEmpty());

  // As per xdg-ouput spec, compositors not scaling the monitor viewport in its
  // compositing space will advertise logical size equal to the physical size
  // (coming from current wl_output's mode info), in which case wl_output's
  // scale must be used. Mutter, for example, when running with its logical
  // monitor layout mode disabled, reports the same value for both logical and
  // physical size even for scales other than 1, which is considered
  // spec-compliant and has been similarly worked around in toolkits like GTK.
  // See https://gitlab.gnome.org/GNOME/mutter/-/issues/2631 for more details.
  //
  // TODO(crbug.com/336007385): Deriving display scale from xdg-ouput logical
  // size has been considered hacky and bug-prone, eg: rounding issues, as the
  // rounding algorithm used by compositors is unspecified. wp-fractional-scale
  // should be used instead as the long term solution, though it requires
  // broader refactor in how scales are assigned to browser windows.
  compute_scale_from_size &=
      (!logical_size.IsEmpty() && logical_size != physical_size);
  if (compute_scale_from_size) {
    const float max_physical_side =
        std::max(physical_size.width(), physical_size.height());
    const float max_logical_side =
        std::max(logical_size.width(), logical_size.height());
    // The scale needs to be clamped here in the same way as in other wayland
    // code, e.g. 'WaylandSurface::GetWaylandScale()'.
    metrics.scale_factor = wl::ClampScale(max_physical_side / max_logical_side);
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
