// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_tablet_seat.h"

#include <tablet-unstable-v2-client-protocol.h>

#include "base/notimplemented.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_tablet.h"
#include "ui/ozone/platform/wayland/host/wayland_tablet_tool.h"

namespace ui {

WaylandTabletSeat::WaylandTabletSeat(zwp_tablet_seat_v2* tablet_seat,
                                     WaylandConnection* connection,
                                     WaylandEventSource* event_source)
    : tablet_seat_(tablet_seat),
      connection_(connection),
      event_source_(event_source) {
  static constexpr zwp_tablet_seat_v2_listener kListener = {
      .tablet_added = &TabletAdded,
      .tool_added = &ToolAdded,
      .pad_added = &PadAdded,
  };
  zwp_tablet_seat_v2_add_listener(tablet_seat_.get(), &kListener, this);
}

WaylandTabletSeat::~WaylandTabletSeat() = default;

// static
void WaylandTabletSeat::TabletAdded(void* data,
                                    zwp_tablet_seat_v2* seat,
                                    zwp_tablet_v2* id) {
  auto* self = static_cast<WaylandTabletSeat*>(data);
  auto tablet = std::make_unique<WaylandTablet>(id, self);
  self->tablets_[tablet->id()] = std::move(tablet);
}

// static
void WaylandTabletSeat::ToolAdded(void* data,
                                  zwp_tablet_seat_v2* seat,
                                  zwp_tablet_tool_v2* id) {
  auto* self = static_cast<WaylandTabletSeat*>(data);
  auto tool = std::make_unique<WaylandTabletTool>(
      id, self, self->connection_, self->event_source_, self->event_source_);
  self->tools_[tool->id()] = std::move(tool);
}

// static
void WaylandTabletSeat::PadAdded(void* data,
                                 zwp_tablet_seat_v2* seat,
                                 zwp_tablet_pad_v2* id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandTabletSeat::OnTabletRemoved(WaylandTablet* tablet) {
  tablets_.erase(tablet->id());
}

void WaylandTabletSeat::OnToolRemoved(WaylandTabletTool* tool) {
  tools_.erase(tool->id());
}

}  // namespace ui
