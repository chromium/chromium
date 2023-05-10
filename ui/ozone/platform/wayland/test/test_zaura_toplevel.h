// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_

#include <aura-shell-server-protocol.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_region.h"

namespace wl {

extern const struct zaura_toplevel_interface kTestZAuraToplevelImpl;

// Manages zaura_toplevel object.
class TestZAuraToplevel : public ServerObject {
 public:
  explicit TestZAuraToplevel(wl_resource* resource);

  TestZAuraToplevel(const TestZAuraToplevel&) = delete;
  TestZAuraToplevel& operator=(const TestZAuraToplevel&) = delete;

  ~TestZAuraToplevel() override;

  // TODO(tluk): `shape_` should really not have a public setter method, the
  // member should instead only be set by the handler that responds to
  // aura_toplevel.set_shape events from the server.
  const absl::optional<TestRegion>& shape() const { return shape_; }
  void set_shape(const absl::optional<TestRegion>& shape) { shape_ = shape; }

 private:
  absl::optional<TestRegion> shape_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_
