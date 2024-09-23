// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_xdg_toplevel_icon.h"

#include <wayland-server-core.h>

#include "base/check_op.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

void DestroyManager(struct wl_client* client, struct wl_resource* resource) {}

void CreateIcon(struct wl_client* client,
                struct wl_resource* resource,
                uint32_t id) {
  auto* global = GetUserDataAs<MockXdgToplevelIconManagerV1>(resource);
  wl_resource* icon = CreateResourceWithImpl<MockXdgToplevelIconV1>(
      client, &xdg_toplevel_icon_v1_interface, 1, &kMockXdgToplevelIconImpl, id,
      global);
  global->set_icon(GetUserDataAs<MockXdgToplevelIconV1>(icon));
}

void SetIcon(struct wl_client* client,
             struct wl_resource* resource,
             struct wl_resource* toplevel,
             struct wl_resource* icon) {
  auto* manager = GetUserDataAs<MockXdgToplevelIconManagerV1>(resource);
  ASSERT_TRUE(manager);
  auto* toplevel_icon = GetUserDataAs<MockXdgToplevelIconV1>(icon);
  ASSERT_TRUE(toplevel_icon);
  manager->resources() = toplevel_icon->resources();
}

}  // namespace

const struct xdg_toplevel_icon_manager_v1_interface
    kMockXdgToplevelIconManagerImpl = {
        .destroy = DestroyManager,
        .create_icon = CreateIcon,
        .set_icon = SetIcon,
};

MockXdgToplevelIconManagerV1::MockXdgToplevelIconManagerV1()
    : GlobalObject(&xdg_toplevel_icon_manager_v1_interface,
                   &kMockXdgToplevelIconManagerImpl,
                   1) {}

MockXdgToplevelIconManagerV1::~MockXdgToplevelIconManagerV1() = default;

namespace {

void DestroyIcon(struct wl_client* client, struct wl_resource* resource) {}

void SetName(struct wl_client* client,
             struct wl_resource* resource,
             const char* icon_name) {}

void AddBuffer(struct wl_client* client,
               struct wl_resource* resource,
               struct wl_resource* buffer,
               int32_t scale) {
  auto* icon = GetUserDataAs<MockXdgToplevelIconV1>(resource);
  ASSERT_TRUE(icon);
  wl_shm_buffer* shm_buffer = wl_shm_buffer_get(buffer);
  ASSERT_TRUE(buffer);
  auto width = wl_shm_buffer_get_width(shm_buffer);
  auto height = wl_shm_buffer_get_height(shm_buffer);
  icon->resources().emplace_back(gfx::Size(width, height), scale);
}

}  // namespace

const struct xdg_toplevel_icon_v1_interface kMockXdgToplevelIconImpl = {
    .destroy = DestroyIcon,
    .set_name = SetName,
    .add_buffer = AddBuffer,
};

MockXdgToplevelIconV1::MockXdgToplevelIconV1(
    wl_resource* resource,
    MockXdgToplevelIconManagerV1* global)
    : ServerObject(resource), global_(global) {}

MockXdgToplevelIconV1::~MockXdgToplevelIconV1() {
  CHECK_EQ(global_->icon(), this);
  global_->set_icon(nullptr);
}

}  // namespace wl
