// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_foreign_wrapper.h"

#include <xdg-foreign-unstable-v1-client-protocol.h>
#include <xdg-foreign-unstable-v2-client-protocol.h>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

constexpr uint32_t kMinVersion = 1;

using OnHandleExported = XdgForeignWrapper::OnHandleExported;

namespace {

template <typename ExporterType>
std::unique_ptr<XdgForeignWrapper> CreateWrapper(WaylandConnection* connection,
                                                 wl_registry* registry,
                                                 uint32_t name,
                                                 uint32_t version) {
  auto zxdg_exporter = wl::Bind<ExporterType>(registry, name, version);
  if (!zxdg_exporter) {
    LOG(ERROR) << "Failed to bind zxdg_exporter";
    return {};
  }
  return std::make_unique<XdgForeignWrapper>(connection,
                                             std::move(zxdg_exporter));
}

}  // namespace

template <typename ExportedType>
struct ExportedSurface {
  ExportedSurface(wl_surface* surface, OnHandleExported cb)
      : surface_for_export(surface) {
    callbacks.emplace_back((std::move(cb)));
  }
  ExportedSurface(ExportedSurface&& buffer) = default;
  ExportedSurface& operator=(ExportedSurface&& buffer) = default;
  ~ExportedSurface() = default;

  // Surface that is exported.
  raw_ptr<wl_surface> surface_for_export = nullptr;

  // Exported |surface|.
  wl::Object<ExportedType> exported;

  // Handle of the exported |surface|.
  std::string exported_handle;

  // The cb that will be executed when |handle| is exported.
  std::vector<OnHandleExported> callbacks;
};

class XdgForeignWrapper::XdgForeignWrapperInternal {
 public:
  virtual ~XdgForeignWrapperInternal() = default;

  virtual void ExportSurfaceToForeign(wl_surface* surface,
                                      OnHandleExported cb) = 0;

  // WaylandWindowObserver:
  virtual void OnWindowRemoved(WaylandWindow* window) = 0;
};

template <typename ExporterType, typename ExportedType>
class XdgForeignWrapperImpl
    : public XdgForeignWrapper::XdgForeignWrapperInternal {
 public:
  XdgForeignWrapperImpl(WaylandConnection* connection,
                        wl::Object<ExporterType> exporter)
      : connection_(connection), exporter_(std::move(exporter)) {}
  XdgForeignWrapperImpl(const XdgForeignWrapperImpl&) = delete;
  XdgForeignWrapperImpl& operator=(const XdgForeignWrapperImpl&) = delete;
  ~XdgForeignWrapperImpl() override = default;

  void ExportSurfaceToForeign(wl_surface* surface,
                              OnHandleExported cb) override {
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
      // The |surface| has already been exported and its handle has been
      // received. Execute the |cb| and send the handle.
      DCHECK(!exported_surface->exported_handle.empty());
      std::move(cb).Run(exported_surface->exported_handle);
    }
  }

  void ExportSurfaceInternal(wl_surface* surface, OnHandleExported cb);

  void OnWindowRemoved(WaylandWindow* window) override {
    auto it = base::ranges::find(
        exported_surfaces_, window->root_surface()->surface(),
        &ExportedSurface<ExportedType>::surface_for_export);
    if (it != exported_surfaces_.end())
      exported_surfaces_.erase(it);
  }

  ExportedSurface<ExportedType>* GetExportedSurface(wl_surface* surface) {
    for (auto& item : exported_surfaces_) {
      if (item.surface_for_export == surface)
        return &item;
    }
    return nullptr;
  }

 private:
  // zxdg_exported_{v1,v2}_listener callbacks:
  static void OnHandle(void* data, ExportedType* exported, const char* handle) {
    auto* self =
        static_cast<XdgForeignWrapperImpl<ExporterType, ExportedType>*>(data);
    DCHECK(self);

    auto exported_surface_it = base::ranges::find(
        self->exported_surfaces_, exported,
        [](const auto& item) { return item.exported.get(); });
    CHECK(exported_surface_it != self->exported_surfaces_.end(),
          base::NotFatalUntil::M130);
    exported_surface_it->exported_handle = handle;

    for (auto& cb : exported_surface_it->callbacks) {
      std::move(cb).Run(exported_surface_it->exported_handle);
    }

    exported_surface_it->callbacks.clear();
  }

  const raw_ptr<WaylandConnection> connection_;
  wl::Object<ExporterType> exporter_;
  std::vector<ExportedSurface<ExportedType>> exported_surfaces_;
};

template <>
void XdgForeignWrapperImpl<zxdg_exporter_v1, zxdg_exported_v1>::
    ExportSurfaceInternal(wl_surface* surface, OnHandleExported cb) {
  ExportedSurface<zxdg_exported_v1> exported_surface(surface, std::move(cb));
  exported_surface.exported.reset(
      zxdg_exporter_v1_export(exporter_.get(), surface));

  static constexpr zxdg_exported_v1_listener kXdgExportedListener = {
      .handle = &OnHandle};
  zxdg_exported_v1_add_listener(exported_surface.exported.get(),
                                &kXdgExportedListener, this);

  exported_surfaces_.emplace_back(std::move(exported_surface));
  connection_->Flush();
}

template <>
void XdgForeignWrapperImpl<zxdg_exporter_v2, zxdg_exported_v2>::
    ExportSurfaceInternal(wl_surface* surface, OnHandleExported cb) {
  ExportedSurface<zxdg_exported_v2> exported_surface(surface, std::move(cb));
  exported_surface.exported.reset(
      zxdg_exporter_v2_export_toplevel(exporter_.get(), surface));

  static constexpr zxdg_exported_v2_listener kXdgExportedListener = {
      .handle = &OnHandle};
  zxdg_exported_v2_add_listener(exported_surface.exported.get(),
                                &kXdgExportedListener, this);

  exported_surfaces_.emplace_back(std::move(exported_surface));
  connection_->Flush();
}

// static
void XdgForeignWrapper::Instantiate(WaylandConnection* connection,
                                    wl_registry* registry,
                                    uint32_t name,
                                    const std::string& interface,
                                    uint32_t version) {
  if (connection->xdg_foreign_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  if (interface == kInterfaceNameV1) {
    connection->xdg_foreign_ = CreateWrapper<zxdg_exporter_v1>(
        connection, registry, name, kMinVersion);
  } else if (interface == kInterfaceNameV2) {
    connection->xdg_foreign_ = CreateWrapper<zxdg_exporter_v2>(
        connection, registry, name, kMinVersion);
  } else {
    NOTREACHED_IN_MIGRATION() << " unexpected interface name: " << interface;
  }
}

XdgForeignWrapper::XdgForeignWrapper(WaylandConnection* connection,
                                     wl::Object<zxdg_exporter_v1> exporter_v1) {
  impl_ = std::make_unique<
      XdgForeignWrapperImpl<zxdg_exporter_v1, zxdg_exported_v1>>(
      connection, std::move(exporter_v1));
}

XdgForeignWrapper::XdgForeignWrapper(WaylandConnection* connection,
                                     wl::Object<zxdg_exporter_v2> exporter_v2) {
  impl_ = std::make_unique<
      XdgForeignWrapperImpl<zxdg_exporter_v2, zxdg_exported_v2>>(
      connection, std::move(exporter_v2));
}

XdgForeignWrapper::~XdgForeignWrapper() = default;

void XdgForeignWrapper::ExportSurfaceToForeign(WaylandWindow* window,
                                               OnHandleExported cb) {
  DCHECK_EQ(window->type(), PlatformWindowType::kWindow);
  auto* surface = window->root_surface()->surface();
  impl_->ExportSurfaceToForeign(surface, std::move(cb));
}

void XdgForeignWrapper::OnWindowRemoved(WaylandWindow* window) {
  impl_->OnWindowRemoved(window);
}

}  // namespace ui
