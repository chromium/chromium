// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"

#include <aura-shell-client-protocol.h>

#include "base/check.h"

namespace ui {

WaylandZAuraOutput::WaylandZAuraOutput(zaura_output* aura_output)
    : obj_(aura_output) {
  DCHECK(obj_);

  static constexpr zaura_output_listener kZAuraOutputListener = {
      &OnScale, &OnConnection, &OnDeviceScaleFactor, &OnInsets,
      &OnLogicalTransform};
  zaura_output_add_listener(obj_.get(), &kZAuraOutputListener, this);
}

WaylandZAuraOutput::~WaylandZAuraOutput() = default;

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

}  // namespace ui
