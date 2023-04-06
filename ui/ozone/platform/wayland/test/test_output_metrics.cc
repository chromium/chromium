// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_output_metrics.h"

namespace wl {
namespace {
int64_t display_id_counter = 10;
}  // namespace

TestOutputMetrics::TestOutputMetrics()
    : aura_display_id(display_id_counter++) {}

TestOutputMetrics::TestOutputMetrics(const gfx::Rect& bounds)
    : wl_physical_size(bounds.size()),
      wl_origin(bounds.origin()),
      xdg_logical_size(bounds.size()),
      xdg_logical_origin(bounds.origin()),
      aura_display_id(display_id_counter++) {}

TestOutputMetrics::TestOutputMetrics(TestOutputMetrics&&) = default;

TestOutputMetrics& TestOutputMetrics::operator=(TestOutputMetrics&&) = default;

TestOutputMetrics::~TestOutputMetrics() = default;

}  // namespace wl
