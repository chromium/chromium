// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_SURFACE1_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_SURFACE1_H_


#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class GtkSurface1 {
 public:
  explicit GtkSurface1(gtk_surface1* surface1);
  GtkSurface1(const GtkSurface1&) = delete;
  GtkSurface1& operator=(const GtkSurface1&) = delete;
  ~GtkSurface1();

  void RequestFocus();

 private:
  wl::Object<gtk_surface1> surface1_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_GTK_SURFACE1_H_
