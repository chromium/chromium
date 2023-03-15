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

  static constexpr zaura_output_listener kZAuraOutputListener = {
      &OnScale,    &OnConnection,       &OnDeviceScaleFactor,
      &OnInsets,   &OnLogicalTransform, &OnDisplayId,
      &OnActivated};
  zaura_output_add_listener(obj_.get(), &kZAuraOutputListener, this);
}

WaylandZAuraOutput::WaylandZAuraOutput() : obj_(nullptr) {}

WaylandZAuraOutput::~WaylandZAuraOutput() = default;

bool WaylandZAuraOutput::IsReady() const {
  // Assume the output is ready regardless of whether the associated server
  // events have arrived if the server is too old to support display_id.
  // TODO(tluk): Determine if this is a correct assumption to make or if we need
  // to update this for older servers.
  return wl::get_version_of_object(obj_.get()) <
             ZAURA_OUTPUT_DISPLAY_ID_SINCE_VERSION ||
         is_ready_;
}

void WaylandZAuraOutput::OnDone() {
  // If `display_id_` has been set the server must have propagated all the
  // necessary state events for this zaura_output.
  is_ready_ = display_id_.has_value();
}

void WaylandZAuraOutput::OnScale(void* data,
                                 struct zaura_output* zaura_output,
                                 uint32_t flags,
                                 uint32_t scale) {}

void WaylandZAuraOutput::OnConnection(void* data,
                                      struct zaura_output* zaura_output,
                                      uint32_t connection) {}

void WaylandZAuraOutput::OnDeviceScaleFactor(void* data,
                                             struct zaura_output* zaura_output,
                                             uint32_t scale) {}

void WaylandZAuraOutput::OnInsets(void* data,
                                  struct zaura_output* zaura_output,
                                  int32_t top,
                                  int32_t left,
                                  int32_t bottom,
                                  int32_t right) {
  if (auto* aura_output = static_cast<WaylandZAuraOutput*>(data))
    aura_output->insets_ = gfx::Insets::TLBR(top, left, bottom, right);
}

void WaylandZAuraOutput::OnLogicalTransform(void* data,
                                            struct zaura_output* zaura_output,
                                            int32_t transform) {
  if (auto* aura_output = static_cast<WaylandZAuraOutput*>(data))
    aura_output->logical_transform_ = transform;
}

void WaylandZAuraOutput::OnDisplayId(void* data,
                                     struct zaura_output* zaura_output,
                                     uint32_t display_id_hi,
                                     uint32_t display_id_lo) {
  if (auto* aura_output = static_cast<WaylandZAuraOutput*>(data)) {
    aura_output->display_id_ =
        ui::wayland::FromWaylandDisplayIdPair({display_id_hi, display_id_lo});
  }
}

void WaylandZAuraOutput::OnActivated(void* data,
                                     struct zaura_output* zaura_output) {
  auto* aura_output = static_cast<WaylandZAuraOutput*>(data);
  if (aura_output && aura_output->IsReady()) {
    DCHECK(display::Screen::GetScreen());
    display::Screen::GetScreen()->SetDisplayForNewWindows(
        aura_output->display_id_.value());
  }
}

}  // namespace ui
