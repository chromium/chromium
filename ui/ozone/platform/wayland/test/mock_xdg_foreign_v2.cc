// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_foreign_v2.h"

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void ExportToplevel(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* surface) {
  auto* exporter = GetUserDataAs<MockXdgExporterV2>(resource);
  wl_resource* exported =
      wl_resource_create(client, &zxdg_exported_v2_interface,
                         wl_resource_get_version(resource), id);
  if (!exported) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(exported, &kMockXdgExportedV2Impl, nullptr,
                                 nullptr);
  zxdg_exported_v2_send_handle(exported, exporter->handle().c_str());
  exporter->ExportToplevel(surface);
}

}  // namespace

const struct zxdg_exported_v2_interface kMockXdgExportedV2Impl = {
    &DestroyResource,  // destroy
};

const struct zxdg_exporter_v2_interface kMockXdgExporterV2Impl = {
    &DestroyResource,  // destroy
    &ExportToplevel,   // export_toplevel
};

MockXdgExporterV2::MockXdgExporterV2()
    : GlobalObject(&zxdg_exporter_v2_interface, &kMockXdgExporterV2Impl, 1) {}

MockXdgExporterV2::~MockXdgExporterV2() = default;

}  // namespace wl
