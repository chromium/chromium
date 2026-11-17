// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_OUTPUT_H_

#include <wayland-client-protocol.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct zxdg_output_v1_interface kTestZXdgOutputImpl;

class TestOutput;
struct TestOutputMetrics;

// Handles the server-side representation of the zxdg_output.
class TestZXdgOutput : public ServerObject {
 public:
  explicit TestZXdgOutput(wl_resource* resource);
  TestZXdgOutput(const TestZXdgOutput&) = delete;
  TestZXdgOutput& operator=(const TestZXdgOutput&) = delete;
  ~TestZXdgOutput() override;

  // Called by the owning wl_output as part of its Flush() operation that
  // propagates the current state of `metrics_` to clients.
  void Flush(const TestOutputMetrics& metrics);

  // Sets the wl_output this zxdg_output belongs to. The two are unlinked when
  // either of them is destroyed.
  void set_output(TestOutput* output) { output_ = output; }

 private:
  raw_ptr<TestOutput> output_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_OUTPUT_H_
