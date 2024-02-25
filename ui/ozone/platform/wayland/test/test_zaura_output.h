// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_H_

#include <aura-shell-server-protocol.h>
#include <wayland-client-protocol.h>

#include <optional>

#include "ui/gfx/geometry/insets.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zaura_output_interface kTestZAuraOutputImpl;

struct TestOutputMetrics;

// Handles the server-side representation of the zaura_output.
class TestZAuraOutput : public ServerObject {
 public:
  explicit TestZAuraOutput(wl_resource* resource);
  TestZAuraOutput(const TestZAuraOutput&) = delete;
  TestZAuraOutput& operator=(const TestZAuraOutput&) = delete;
  ~TestZAuraOutput() override;

  // Sends the activated event immediately.
  void SendActivated();

  // Called by the owning wl_output as part of its Flush() operation that
  // propagates the current state of `metrics_` to clients.
  void Flush(const TestOutputMetrics& metrics);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_H_
