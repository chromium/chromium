// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_viewporter.h"

#include <viewporter-server-protocol.h>
#include <wayland-server-core.h>

#include "base/check.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_viewport.h"

namespace wl {

namespace {

constexpr uint32_t kViewporterVersion = 1;

void GetViewport(struct wl_client* client,
                 struct wl_resource* resource,
                 uint32_t id,
                 struct wl_resource* surface) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->viewport()) {
    wl_resource_post_error(resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                           "viewport exists");
    return;
  }

  wl_resource* viewport_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestViewport>>(
          client, &wp_viewport_interface, wl_resource_get_version(resource),
          &kTestViewportImpl, id, surface);
  DCHECK(viewport_resource);
  mock_surface->set_viewport(GetUserDataAs<TestViewport>(viewport_resource));
}

}  // namespace

const struct wp_viewporter_interface kTestViewporterImpl = {
    DestroyResource,
    GetViewport,
};

TestViewporter::TestViewporter()
    : GlobalObject(&wp_viewporter_interface,
                   &kTestViewporterImpl,
                   kViewporterVersion) {}

TestViewporter::~TestViewporter() = default;

}  // namespace wl
