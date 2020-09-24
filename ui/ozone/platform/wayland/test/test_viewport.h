// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_VIEWPORT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_VIEWPORT_H_

#include <viewporter-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wp_viewport_interface kTestViewportImpl;

class TestViewport : public ServerObject {
 public:
  explicit TestViewport(wl_resource* resource, wl_resource* surface);
  ~TestViewport() override;
  TestViewport(const TestViewport& rhs) = delete;
  TestViewport& operator=(const TestViewport& rhs) = delete;

 private:
  // Surface resource that is the ground for this Viewport.
  wl_resource* surface_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_VIEWPORT_H_
