// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_FOREIGN_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_FOREIGN_WRAPPER_H_

#include <string>

#include "base/callback.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class WaylandConnection;

// A wrapper for xdg foreign objects. Exports surfaces that have xdg_surface
// roles and asynchronously returns handles for them. Only xdg_surface surfaces
// may be exported. Currently supports only exporting surfaces.
//
// TODO(1126817): consider supporting xdg-foreign-v2.
class XdgForeignWrapper : public WaylandWindowObserver {
 public:
  using OnHandleExported = base::OnceCallback<void(const std::string&)>;

  XdgForeignWrapper(WaylandConnection* connection,
                    wl::Object<zxdg_exporter_v1> exporter_v1);
  XdgForeignWrapper(const XdgForeignWrapper&) = delete;
  XdgForeignWrapper& operator=(const XdgForeignWrapper&) = delete;
  ~XdgForeignWrapper() override;

  // Exports |window|'s wl_surface and asynchronously returns a handle for that
  // via the |cb|. Please note that wl_surface that has xdg_surface role can be
  // exported.
  void ExportSurfaceToForeign(WaylandWindow* window, OnHandleExported cb);

 private:
  struct ExportedSurface;

  ExportedSurface* GetExportedSurface(wl_surface* surface);

  void ExportSurfaceInternal(wl_surface* surface, OnHandleExported cb);

  // WaylandWindowObserver:
  void OnWindowRemoved(WaylandWindow* window) override;

  // zxdg_exported_v1_listener:
  static void OnExported(void* data,
                         zxdg_exported_v1* exported,
                         const char* handle);

  WaylandConnection* const connection_;

  wl::Object<zxdg_exporter_v1> exporter_v1_;

  std::vector<ExportedSurface> exported_surfaces_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_FOREIGN_WRAPPER_H_
