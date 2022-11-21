// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_shell1.h"

#include <gtk-shell-client-protocol.h>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "ui/ozone/platform/wayland/host/gtk_surface1.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
// gtk_shell1 exposes request_focus() since version 3.  Below that, it is not
// interesting for us, although it provides some shell integration that might be
// useful.
constexpr uint32_t kMinVersion = 3;
constexpr uint32_t kMaxVersion = 4;
}  // namespace

// static
constexpr char GtkShell1::kInterfaceName[];

// static
void GtkShell1::Instantiate(WaylandConnection* connection,
                            wl_registry* registry,
                            uint32_t name,
                            const std::string& interface,
                            uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->gtk_shell1_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto gtk_shell1 =
      wl::Bind<::gtk_shell1>(registry, name, std::min(version, kMaxVersion));
  if (!gtk_shell1) {
    LOG(ERROR) << "Failed to bind gtk_shell1";
    return;
  }
  connection->gtk_shell1_ = std::make_unique<GtkShell1>(gtk_shell1.release());
  ReportShellUMA(UMALinuxWaylandShell::kGtkShell1);
}

GtkShell1::GtkShell1(gtk_shell1* shell1) : shell1_(shell1) {}

GtkShell1::~GtkShell1() = default;

std::unique_ptr<GtkSurface1> GtkShell1::GetGtkSurface1(
    wl_surface* top_level_window_surface) {
  return std::make_unique<GtkSurface1>(
      gtk_shell1_get_gtk_surface(shell1_.get(), top_level_window_surface));
}

void GtkShell1::SetStartupId(const std::string& startup_id) {
  gtk_shell1_set_startup_id(shell1_.get(), startup_id.c_str());
}

}  // namespace ui
