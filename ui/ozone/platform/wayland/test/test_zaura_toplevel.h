// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_

#include <aura-shell-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zaura_toplevel_interface kTestZAuraToplevelImpl;

// Manages zaura_toplevel object.
class TestZAuraToplevel : public ServerObject {
 public:
  explicit TestZAuraToplevel(wl_resource* resource);

  TestZAuraToplevel(const TestZAuraToplevel&) = delete;
  TestZAuraToplevel& operator=(const TestZAuraToplevel&) = delete;

  ~TestZAuraToplevel() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_
