// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zxdg_output_manager.h"

#include <xdg-output-unstable-v1-server-protocol.h>
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_zxdg_output.h"

namespace wl {

namespace {

constexpr uint32_t kZXdgOutputManagerVersion = 3;
constexpr uint32_t kZXdgOutputVersion = 3;

void GetXdgOutput(wl_client* client,
                  wl_resource* resource,
                  uint32_t id,
                  wl_resource* output_resource) {
  wl_resource* zxdg_output_resource = CreateResourceWithImpl<TestZXdgOutput>(
      client, &zxdg_output_v1_interface, kZXdgOutputVersion,
      &kTestZXdgOutputImpl, id);
  auto* output = GetUserDataAs<TestOutput>(output_resource);
  output->SetXdgOutput(GetUserDataAs<TestZXdgOutput>(zxdg_output_resource));
}

const struct zxdg_output_manager_v1_interface kTestZXdgOutputManagerImpl = {
    &DestroyResource,
    &GetXdgOutput,
};

}  // namespace

TestZXdgOutputManager::TestZXdgOutputManager()
    : GlobalObject(&zxdg_output_manager_v1_interface,
                   &kTestZXdgOutputManagerImpl,
                   kZXdgOutputManagerVersion) {}

TestZXdgOutputManager::~TestZXdgOutputManager() = default;

}  // namespace wl
