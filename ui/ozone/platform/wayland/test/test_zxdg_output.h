// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_OUTPUT_H_

#include <wayland-client-protocol.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zxdg_output_v1_interface kTestZXdgOutputImpl;

// Manages zxdg_output object.
class TestZXdgOutput : public ServerObject {
 public:
  explicit TestZXdgOutput(wl_resource* resource);

  TestZXdgOutput(const TestZXdgOutput&) = delete;
  TestZXdgOutput& operator=(const TestZXdgOutput&) = delete;

  ~TestZXdgOutput() override;

  void SetLogicalSize(const gfx::Size& size);

  // Send logical size w/o remembering.
  void SendLogicalSize(const gfx::Size& size);

  bool HasLogicalSize() const {
    return logical_size_.has_value() || pending_logical_size_.has_value();
  }

  void Flush();

 private:
  absl::optional<gfx::Size> pending_logical_size_;
  absl::optional<gfx::Size> logical_size_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_OUTPUT_H_
