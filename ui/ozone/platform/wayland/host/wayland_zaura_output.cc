// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"

#include <aura-shell-client-protocol.h>

#include "base/check.h"
#include "base/logging.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/screen.h"

namespace ui {

WaylandZAuraOutput::WaylandZAuraOutput(zaura_output* aura_output)
    : obj_(aura_output) {
  DCHECK(obj_);
  static constexpr zaura_output_listener kAuraOutputListener = {
      .scale = &OnScale,
      .connection = &OnConnection,
      .device_scale_factor = &OnDeviceScaleFactor,
      .insets = &OnInsets,
      .logical_transform = &OnLogicalTransform,
      .display_id = &OnDisplayId,
      .activated = &OnActivated};
  zaura_output_add_listener(obj_.get(), &kAuraOutputListener, this);
}

WaylandZAuraOutput::WaylandZAuraOutput() : obj_(nullptr) {}

WaylandZAuraOutput::~WaylandZAuraOutput() = default;

bool WaylandZAuraOutput::IsReady() const {
  return is_ready_;
}

void WaylandZAuraOutput::OnDone() {
  // If `display_id_` has been set the server must have propagated all the
  // necessary state events for this zaura_output.
  is_ready_ = display_id_.has_value();
}

void WaylandZAuraOutput::UpdateMetrics(WaylandOutput::Metrics& metrics) {
  if (!IsReady()) {
    return;
  }

  metrics.insets = insets_;
  metrics.logical_transform = logical_transform_.value();
  metrics.display_id = display_id_.value();
}

void WaylandZAuraOutput::OnScale(void* data,
                                 zaura_output* output,
                                 uint32_t flags,
                                 uint32_t scale) {}

void WaylandZAuraOutput::OnConnection(void* data,
                                      zaura_output* output,
                                      uint32_t connection) {}

void WaylandZAuraOutput::OnDeviceScaleFactor(void* data,
                                             zaura_output* output,
                                             uint32_t scale) {}

void WaylandZAuraOutput::OnInsets(void* data,
                                  zaura_output* output,
                                  int32_t top,
                                  int32_t left,
                                  int32_t bottom,
                                  int32_t right) {
  if (auto* self = static_cast<WaylandZAuraOutput*>(data)) {
    self->insets_ = gfx::Insets::TLBR(top, left, bottom, right);
  }
}

void WaylandZAuraOutput::OnLogicalTransform(void* data,
                                            zaura_output* output,
                                            int32_t transform) {
  if (auto* self = static_cast<WaylandZAuraOutput*>(data)) {
    self->logical_transform_ = transform;
  }
}

void WaylandZAuraOutput::OnDisplayId(void* data,
                                     zaura_output* output,
                                     uint32_t display_id_hi,
                                     uint32_t display_id_lo) {
  if (auto* self = static_cast<WaylandZAuraOutput*>(data)) {
    self->display_id_ =
        ui::wayland::FromWaylandDisplayIdPair({display_id_hi, display_id_lo});
  }
}

void WaylandZAuraOutput::OnActivated(void* data, zaura_output* output) {
  auto* self = static_cast<WaylandZAuraOutput*>(data);
  if (self && self->IsReady()) {
    DCHECK(display::Screen::GetScreen());
    display::Screen::GetScreen()->SetDisplayForNewWindows(
        self->display_id_.value());
  }
}

}  // namespace ui
