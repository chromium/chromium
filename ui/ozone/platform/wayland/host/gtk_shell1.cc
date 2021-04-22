// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_shell1.h"

#include <gtk-shell-client-protocol.h>

#include "ui/ozone/platform/wayland/host/gtk_surface1.h"

namespace ui {

GtkShell1::GtkShell1(gtk_shell1* shell1) : shell1_(shell1) {}

GtkShell1::~GtkShell1() = default;

std::unique_ptr<GtkSurface1> GtkShell1::GetGtkSurface1(
    wl_surface* top_level_window_surface) {
  return std::make_unique<GtkSurface1>(
      gtk_shell1_get_gtk_surface(shell1_.get(), top_level_window_surface));
}

}  // namespace ui
