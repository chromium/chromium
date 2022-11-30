// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_surface1.h"

#include <gtk-shell-client-protocol.h>

#include "base/logging.h"

namespace ui {

GtkSurface1::GtkSurface1(gtk_surface1* surface1) : surface1_(surface1) {}

GtkSurface1::~GtkSurface1() = default;

void GtkSurface1::RequestFocus() {
  gtk_surface1_request_focus(surface1_.get(), nullptr);
}

}  // namespace ui
