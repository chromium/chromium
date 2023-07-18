// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_surface_augmenter.h"

#include <surface-augmenter-server-protocol.h>

#include "base/files/scoped_file.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_augmented_subsurface.h"
#include "ui/ozone/platform/wayland/test/test_augmented_surface.h"
#include "ui/ozone/platform/wayland/test/test_buffer.h"

namespace wl {

namespace {

constexpr uint32_t kSurfaceAugmenterProtocolVersion = 7;

void CreateSolidColorBuffer(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t id,
                            struct wl_array* color,
                            int32_t width,
                            int32_t height) {
  std::vector<base::ScopedFD> fds;
  CreateResourceWithImpl<::testing::NiceMock<TestBuffer>>(
      client, &wl_buffer_interface, wl_resource_get_version(resource),
      &kTestWlBufferImpl, id, std::move(fds));
}

void GetGetAugmentedSurface(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t id,
                            struct wl_resource* surface) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->augmented_surface()) {
    wl_resource_post_error(resource,
                           SURFACE_AUGMENTER_ERROR_AUGMENTED_SURFACE_EXISTS,
                           "surface_augmenter exists");
    return;
  }

  wl_resource* augmented_surface_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestAugmentedSurface>>(
          client, &augmented_surface_interface,
          wl_resource_get_version(resource), &kTestAugmentedSurfaceImpl, id,
          surface);
  DCHECK(augmented_surface_resource);
  mock_surface->set_augmented_surface(
      GetUserDataAs<TestAugmentedSurface>(augmented_surface_resource));
}

void GetAugmentedSubsurface(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t id,
                            struct wl_resource* subsurface) {
  auto* test_subsurface = GetUserDataAs<TestSubSurface>(subsurface);
  if (test_subsurface->augmented_subsurface()) {
    wl_resource_post_error(resource,
                           SURFACE_AUGMENTER_ERROR_AUGMENTED_SURFACE_EXISTS,
                           "augmented_subsurface exists");
    return;
  }

  wl_resource* augmented_subsurface_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestAugmentedSubSurface>>(
          client, &augmented_sub_surface_interface,
          wl_resource_get_version(resource), &kTestAugmentedSubSurfaceImpl, id,
          subsurface);
  DCHECK(augmented_subsurface_resource);
  test_subsurface->set_augmented_subsurface(
      GetUserDataAs<TestAugmentedSubSurface>(augmented_subsurface_resource));
}

}  // namespace

const struct surface_augmenter_interface kTestSurfaceAugmenterImpl = {
    DestroyResource,
    CreateSolidColorBuffer,
    GetGetAugmentedSurface,
    GetAugmentedSubsurface,
};

TestSurfaceAugmenter::TestSurfaceAugmenter()
    : GlobalObject(&surface_augmenter_interface,
                   &kTestSurfaceAugmenterImpl,
                   kSurfaceAugmenterProtocolVersion) {}

TestSurfaceAugmenter::~TestSurfaceAugmenter() = default;

}  // namespace wl
