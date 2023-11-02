// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ALPHA_BLENDING_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ALPHA_BLENDING_H_

#include <alpha-compositing-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct zcr_blending_v1_interface kTestAlphaBlendingImpl;

class TestAlphaBlending : public ServerObject {
 public:
  explicit TestAlphaBlending(wl_resource* resource, wl_resource* surface);
  TestAlphaBlending(const TestAlphaBlending& rhs) = delete;
  TestAlphaBlending& operator=(const TestAlphaBlending& rhs) = delete;
  ~TestAlphaBlending() override;

 private:
  // Surface resource that is the ground for this Viewport.
  raw_ptr<wl_resource> surface_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ALPHA_BLENDING_H_
