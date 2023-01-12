// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_FOREIGN_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_FOREIGN_WRAPPER_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class WaylandConnection;

// A wrapper for xdg foreign objects. Exports surfaces that have xdg_surface
// roles and asynchronously returns handles for them. Only xdg_surface surfaces
// may be exported. Currently supports only exporting surfaces.
class XdgForeignWrapper : public wl::GlobalObjectRegistrar<XdgForeignWrapper>,
                          public WaylandWindowObserver {
 public:
  class XdgForeignWrapperInternal;

  static constexpr char kInterfaceNameV1[] = "zxdg_exporter_v1";
  static constexpr char kInterfaceNameV2[] = "zxdg_exporter_v2";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  using OnHandleExported = base::OnceCallback<void(const std::string&)>;

  XdgForeignWrapper(WaylandConnection* connection,
                    wl::Object<zxdg_exporter_v1> exporter_v1);
  XdgForeignWrapper(WaylandConnection* connection,
                    wl::Object<zxdg_exporter_v2> exporter_v2);
  XdgForeignWrapper(const XdgForeignWrapper&) = delete;
  XdgForeignWrapper& operator=(const XdgForeignWrapper&) = delete;
  ~XdgForeignWrapper() override;

  // Exports |window|'s wl_surface and asynchronously returns a handle for that
  // via the |cb|. Please note that wl_surface that has xdg_surface role can be
  // exported.
  void ExportSurfaceToForeign(WaylandWindow* window, OnHandleExported cb);

 private:
  // WaylandWindowObserver:
  void OnWindowRemoved(WaylandWindow* window) override;

  std::unique_ptr<XdgForeignWrapperInternal> impl_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_FOREIGN_WRAPPER_H_
