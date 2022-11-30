// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_popup.h"
#include "ui/ozone/platform/wayland/test/test_zaura_surface.h"
#include "ui/ozone/platform/wayland/test/test_zaura_toplevel.h"

namespace wl {

namespace {

constexpr uint32_t kZAuraShellVersion = 42;
constexpr uint32_t kZAuraOutputVersion = 38;

void GetAuraSurface(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* surface_resource) {
  CreateResourceWithImpl<TestZAuraSurface>(client, &zaura_surface_interface,
                                           kZAuraShellVersion,
                                           &kTestZAuraSurfaceImpl, id);
}

void GetAuraOutput(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* output_resource) {
  wl_resource* zaura_output_resource = CreateResourceWithImpl<TestZAuraOutput>(
      client, &zaura_output_interface, kZAuraOutputVersion,
      &kTestZAuraToplevelImpl, id);
  auto* output = GetUserDataAs<TestOutput>(output_resource);
  output->SetAuraOutput(GetUserDataAs<TestZAuraOutput>(zaura_output_resource));
}

void SurfaceSubmissionInPixelCoordinates(wl_client* client,
                                         wl_resource* resource) {
  // TODO(crbug.com/1346347): Implement zaura-shell protocol requests and test
  // their usage.
  NOTIMPLEMENTED_LOG_ONCE();
}

void GetAuraToplevelForXdgToplevel(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t id,
                                   wl_resource* toplevel) {
  CreateResourceWithImpl<TestZAuraToplevel>(client, &zaura_toplevel_interface,
                                            kZAuraShellVersion,
                                            &kTestZAuraToplevelImpl, id);
}

void GetAuraPopupForXdgPopup(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* popup) {
  CreateResourceWithImpl<TestZAuraPopup>(client, &zaura_popup_interface,
                                         kZAuraShellVersion,
                                         &kTestZAuraPopupImpl, id);
}

const struct zaura_shell_interface kMockZAuraShellImpl = {
    &GetAuraSurface,
    &GetAuraOutput,
    &SurfaceSubmissionInPixelCoordinates,
    &GetAuraToplevelForXdgToplevel,
    &GetAuraPopupForXdgPopup,
    &DestroyResource,
};

}  // namespace

MockZAuraShell::MockZAuraShell()
    : GlobalObject(&zaura_shell_interface,
                   &kMockZAuraShellImpl,
                   kZAuraShellVersion) {}

MockZAuraShell::~MockZAuraShell() = default;

}  // namespace wl
