// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_output_manager.h"

#include <aura-shell-server-protocol.h>

#include "base/bit_cast.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_output_metrics.h"

namespace wl {

namespace {
constexpr uint32_t kZAuraOutputManagerVersion = 2;

}  // namespace

TestZAuraOutputManager::TestZAuraOutputManager()
    : GlobalObject(&zaura_output_manager_interface,
                   nullptr,
                   kZAuraOutputManagerVersion) {}

TestZAuraOutputManager::~TestZAuraOutputManager() = default;

void TestZAuraOutputManager::SendOutputMetrics(
    TestOutput* test_output,
    const TestOutputMetrics& metrics) {
  wl_resource* output_resource = test_output->resource();

  const auto& physical_size = metrics.wl_physical_size;
  zaura_output_manager_send_physical_size(resource(), output_resource,
                                          physical_size.width(),
                                          physical_size.height());

  zaura_output_manager_send_panel_transform(resource(), output_resource,
                                            metrics.wl_panel_transform);

  const auto& logical_size = metrics.xdg_logical_size;
  zaura_output_manager_send_logical_size(
      resource(), output_resource, logical_size.width(), logical_size.height());

  const auto& logical_origin = metrics.xdg_logical_origin;
  zaura_output_manager_send_logical_position(
      resource(), output_resource, logical_origin.x(), logical_origin.y());

  const auto display_id =
      ui::wayland::ToWaylandDisplayIdPair(metrics.aura_display_id);
  zaura_output_manager_send_display_id(resource(), output_resource,
                                       display_id.high, display_id.low);

  const auto& insets = metrics.aura_logical_insets;
  zaura_output_manager_send_insets(resource(), output_resource, insets.top(),
                                   insets.left(), insets.bottom(),
                                   insets.right());

  zaura_output_manager_send_device_scale_factor(
      resource(), output_resource,
      base::bit_cast<uint32_t>(metrics.aura_device_scale_factor));

  zaura_output_manager_send_logical_transform(resource(), output_resource,
                                              metrics.aura_logical_transform);

  zaura_output_manager_send_done(resource(), output_resource);
}

void TestZAuraOutputManager::SendActivated(TestOutput* test_output) {
  wl_resource* output_resource = test_output->resource();
  zaura_output_manager_send_activated(resource(), output_resource);
}

}  // namespace wl
