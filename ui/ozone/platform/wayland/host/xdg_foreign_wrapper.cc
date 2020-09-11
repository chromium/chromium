// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"

#include <xdg-foreign-unstable-v1-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

struct XdgForeignWrapper::ExportedSurface {
  ExportedSurface(wl_surface* surface, OnHandleExported cb);
  ExportedSurface(ExportedSurface&& buffer);
  ExportedSurface& operator=(ExportedSurface&& buffer);
  ~ExportedSurface();

  // Surface that is exported.
  wl_surface* surface_for_export = nullptr;

  // Exported |surface|.
  wl::Object<zxdg_exported_v1> exported;

  // Handle of the exported |surface|.
  std::string exported_handle;

  // The cb that will be executed when |handle| is exported.
  std::vector<OnHandleExported> callbacks;
};

XdgForeignWrapper::ExportedSurface::ExportedSurface(wl_surface* surface,
                                                    OnHandleExported cb)
    : surface_for_export(surface) {
  callbacks.emplace_back((std::move(cb)));
}

XdgForeignWrapper::ExportedSurface::ExportedSurface(ExportedSurface&& buffer) =
    default;

XdgForeignWrapper::ExportedSurface&
XdgForeignWrapper::ExportedSurface::operator=(ExportedSurface&& buffer) =
    default;

XdgForeignWrapper::ExportedSurface::~ExportedSurface() = default;

XdgForeignWrapper::XdgForeignWrapper(WaylandConnection* connection,
                                     wl::Object<zxdg_exporter_v1> exporter_v1)
    : connection_(connection), exporter_v1_(std::move(exporter_v1)) {}

XdgForeignWrapper::~XdgForeignWrapper() = default;

void XdgForeignWrapper::ExportSurfaceToForeign(WaylandWindow* window,
                                               OnHandleExported cb) {
  DCHECK_EQ(window->type(), PlatformWindowType::kWindow);
  auto* surface = window->root_surface()->surface();
  auto* exported_surface = GetExportedSurface(surface);
  if (!exported_surface) {
    // The |surface| has never been exported. Export it and return the handle
    // via the |cb|.
    ExportSurfaceInternal(surface, std::move(cb));
  } else if (exported_surface->exported_handle.empty()) {
    // The |surface| has already been exported, but its handle hasn't been
    // received yet. Store the |cb| and execute when the handle is obtained.
    exported_surface->callbacks.emplace_back(std::move(cb));
  } else {
    // The |surface| has already been exported and its handle has been received.
    // Execute the |cb| and send the handle.
    DCHECK(!exported_surface->exported_handle.empty());
    std::move(cb).Run(exported_surface->exported_handle);
  }
}

XdgForeignWrapper::ExportedSurface* XdgForeignWrapper::GetExportedSurface(
    wl_surface* surface) {
  for (auto& item : exported_surfaces_) {
    if (item.surface_for_export == surface)
      return &item;
  }
  return nullptr;
}

void XdgForeignWrapper::ExportSurfaceInternal(wl_surface* surface,
                                              OnHandleExported cb) {
  static const struct zxdg_exported_v1_listener kExportedListener = {
      &XdgForeignWrapper::OnExported};

  ExportedSurface exported_surface(surface, std::move(cb));
  exported_surface.exported.reset(
      zxdg_exporter_v1_export(exporter_v1_.get(), surface));
  zxdg_exported_v1_add_listener(exported_surface.exported.get(),
                                &kExportedListener, this);
  exported_surfaces_.emplace_back(std::move(exported_surface));
  connection_->ScheduleFlush();
}

void XdgForeignWrapper::OnWindowRemoved(WaylandWindow* window) {
  auto it = std::find_if(exported_surfaces_.begin(), exported_surfaces_.end(),
                         [window](const auto& surface) {
                           return window->root_surface()->surface() ==
                                  surface.surface_for_export;
                         });
  if (it != exported_surfaces_.end())
    exported_surfaces_.erase(it);
}

// static
void XdgForeignWrapper::OnExported(void* data,
                                   zxdg_exported_v1* exported,
                                   const char* handle) {
  auto* self = static_cast<XdgForeignWrapper*>(data);
  DCHECK(self);

  auto exported_surface_it = std::find_if(
      self->exported_surfaces_.begin(), self->exported_surfaces_.end(),
      [exported](const auto& item) { return item.exported.get() == exported; });
  DCHECK(exported_surface_it != self->exported_surfaces_.end());
  exported_surface_it->exported_handle = handle;

  for (auto& cb : exported_surface_it->callbacks)
    std::move(cb).Run(exported_surface_it->exported_handle);

  exported_surface_it->callbacks.clear();
}

}  // namespace ui
