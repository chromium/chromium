// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_tablet.h"

#include <tablet-unstable-v2-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_tablet_seat.h"

namespace ui {

WaylandTablet::WaylandTablet(zwp_tablet_v2* tablet, WaylandTabletSeat* seat)
    : tablet_(tablet), seat_(seat) {
  DCHECK(seat_);
  static constexpr zwp_tablet_v2_listener kListener = {
      .name = &Name,
      .id = &Id,
      .path = &Path,
      .done = &Done,
      .removed = &Removed,
  };
  zwp_tablet_v2_add_listener(tablet_.get(), &kListener, this);
}

WaylandTablet::~WaylandTablet() = default;

// static
void WaylandTablet::Name(void* data, zwp_tablet_v2* tablet, const char* name) {
  auto* self = static_cast<WaylandTablet*>(data);
  self->name_ = name;
}

// static
void WaylandTablet::Id(void* data,
                       zwp_tablet_v2* tablet,
                       uint32_t vid,
                       uint32_t pid) {
  auto* self = static_cast<WaylandTablet*>(data);
  self->vid_ = vid;
  self->pid_ = pid;
}

// static
void WaylandTablet::Path(void* data, zwp_tablet_v2* tablet, const char* path) {
  // The path is not currently used by Chromium.
}

// static
void WaylandTablet::Done(void* data, zwp_tablet_v2* tablet) {
  // "done" event is a signal that all static information has been sent.
}

// static
void WaylandTablet::Removed(void* data, zwp_tablet_v2* tablet) {
  auto* self = static_cast<WaylandTablet*>(data);
  self->seat_->OnTabletRemoved(self);
}

}  // namespace ui
