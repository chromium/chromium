// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_H_

#include <wayland-client-protocol.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

// Manages zaura_output object.
class TestZAuraOutput : public ServerObject {
 public:
  explicit TestZAuraOutput(wl_resource* resource);

  TestZAuraOutput(const TestZAuraOutput&) = delete;
  TestZAuraOutput& operator=(const TestZAuraOutput&) = delete;

  ~TestZAuraOutput() override;

  const gfx::Insets& GetInsets() const { return insets_; }
  void SetInsets(const gfx::Insets& insets) { pending_insets_ = insets; }

  int32_t GetLogicalTransform() const { return logical_transform_; }
  void SetLogicalTransform(int32_t logical_transform) {
    pending_logical_transform_ = logical_transform;
  }

  void Flush();

 private:
  gfx::Insets insets_;
  absl::optional<gfx::Insets> pending_insets_;

  int32_t logical_transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  absl::optional<int32_t> pending_logical_transform_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_H_
