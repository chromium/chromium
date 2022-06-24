// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"

#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"

namespace wl {

namespace {

constexpr uint32_t kZAuraShellVersion = 35;
constexpr uint32_t kZAuraOutputVersion = 34;

void GetAuraSurface(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* surface_resource) {}

void GetAuraOutput(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* output_resource) {
  wl_resource* zaura_output_resource = CreateResourceWithImpl<TestZAuraOutput>(
      client, &zaura_output_interface, kZAuraOutputVersion, nullptr, id);
  auto* output = GetUserDataAs<TestOutput>(output_resource);
  output->SetAuraOutput(GetUserDataAs<TestZAuraOutput>(zaura_output_resource));
}

void SurfaceSubmissionInPixelCoordinates(wl_client* client,
                                         wl_resource* resource) {}

const struct zaura_shell_interface kMockZAuraShellImpl = {
    &GetAuraSurface, &GetAuraOutput, &SurfaceSubmissionInPixelCoordinates};

}  // namespace

MockZAuraShell::MockZAuraShell()
    : GlobalObject(&zaura_shell_interface,
                   &kMockZAuraShellImpl,
                   kZAuraShellVersion) {}

MockZAuraShell::~MockZAuraShell() = default;

}  // namespace wl
