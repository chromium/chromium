// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_input_method_context_factory.h"

#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_input_method_context.h"

namespace ui {

WaylandInputMethodContextFactory::WaylandInputMethodContextFactory(
    WaylandConnection* connection)
    : connection_(connection) {
  LinuxInputMethodContextFactory::SetInstance(this);
}

WaylandInputMethodContextFactory::~WaylandInputMethodContextFactory() {
  LinuxInputMethodContextFactory::SetInstance(nullptr);
}

std::unique_ptr<LinuxInputMethodContext>
WaylandInputMethodContextFactory::CreateInputMethodContext(
    LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  return CreateWaylandInputMethodContext(delegate, is_simple);
}

std::unique_ptr<WaylandInputMethodContext>
WaylandInputMethodContextFactory::CreateWaylandInputMethodContext(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple) const {
  return std::make_unique<WaylandInputMethodContext>(
      connection_, delegate, is_simple,
      base::BindRepeating(&WaylandConnection::DispatchUiEvent,
                          base::Unretained(connection_)));
}

}  // namespace ui
