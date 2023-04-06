// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_METRICS_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_METRICS_H_

#include <wayland-server-protocol-core.h>

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace wl {

// Metrics for testing wayland_output and its extensions.
struct TestOutputMetrics {
  // Creates a test metrics object with reasonable defaults.
  TestOutputMetrics();
  // Creates a test metrics object with physical and logical bounds set to
  // `bounds`.
  explicit TestOutputMetrics(const gfx::Rect& bounds);
  TestOutputMetrics(const TestOutputMetrics&) = delete;
  TestOutputMetrics& operator=(const TestOutputMetrics&) = delete;
  TestOutputMetrics(TestOutputMetrics&&);
  TestOutputMetrics& operator=(TestOutputMetrics&&);
  virtual ~TestOutputMetrics();

  // Output metrics
  gfx::Size wl_physical_size = gfx::Size(800, 600);
  gfx::Point wl_origin;
  int32_t wl_scale = 1;
  wl_output_transform wl_panel_transform = WL_OUTPUT_TRANSFORM_NORMAL;

  // ZxdgOutput metrics
  gfx::Size xdg_logical_size = gfx::Size(800, 600);
  gfx::Point xdg_logical_origin;

  // AuraOutput metrics
  int64_t aura_display_id;
  gfx::Insets aura_logical_insets;
  float aura_device_scale_factor = 1;
  wl_output_transform aura_logical_transform = WL_OUTPUT_TRANSFORM_NORMAL;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_METRICS_H_
