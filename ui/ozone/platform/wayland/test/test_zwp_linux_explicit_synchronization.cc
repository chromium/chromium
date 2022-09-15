// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zwp_linux_explicit_synchronization.h"

#include <linux-explicit-synchronization-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "base/check.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"

namespace wl {

namespace {

constexpr uint32_t kLinuxExplicitSynchronizationVersion = 1;

void GetSynchronization(wl_client* client,
                        wl_resource* resource,
                        uint32_t id,
                        wl_resource* surface_resource) {
  CreateResourceWithImpl<TestLinuxSurfaceSynchronization>(
      client, &zwp_linux_surface_synchronization_v1_interface, 1,
      &kMockZwpLinuxSurfaceSynchronizationImpl, id, surface_resource);
}

}  // namespace

const struct zwp_linux_explicit_synchronization_v1_interface
    kTestLinuxExplicitSynchronizationImpl = {DestroyResource,
                                             GetSynchronization};

TestLinuxSurfaceSynchronization::TestLinuxSurfaceSynchronization(
    wl_resource* resource,
    wl_resource* surface_resource)
    : ServerObject(resource), surface_resource_(surface_resource) {}

TestLinuxSurfaceSynchronization::~TestLinuxSurfaceSynchronization() = default;

TestZwpLinuxExplicitSynchronizationV1::TestZwpLinuxExplicitSynchronizationV1()
    : GlobalObject(&zwp_linux_explicit_synchronization_v1_interface,
                   &kTestLinuxExplicitSynchronizationImpl,
                   kLinuxExplicitSynchronizationVersion) {}

TestZwpLinuxExplicitSynchronizationV1::
    ~TestZwpLinuxExplicitSynchronizationV1() = default;

}  // namespace wl
