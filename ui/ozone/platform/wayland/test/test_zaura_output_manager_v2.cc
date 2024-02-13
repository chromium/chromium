// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_output_manager_v2.h"

#include <aura-output-management-server-protocol.h>

#include "base/bit_cast.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_output_metrics.h"

namespace wl {

namespace {
constexpr uint32_t kZAuraOutputManagerV2Version = 1;
}  // namespace

TestZAuraOutputManagerV2::TestZAuraOutputManagerV2()
    : GlobalObject(&zaura_output_manager_v2_interface,
                   nullptr,
                   kZAuraOutputManagerV2Version) {}

TestZAuraOutputManagerV2::~TestZAuraOutputManagerV2() = default;

void TestZAuraOutputManagerV2::SendOutputMetrics(
    TestOutput* test_output,
    const TestOutputMetrics& metrics) {
  const uint64_t output_name =
      test_output->GetOutputName(wl_resource_get_client(resource()));

  const auto display_id =
      ui::wayland::ToWaylandDisplayIdPair(metrics.aura_display_id);
  zaura_output_manager_v2_send_display_id(resource(), output_name,
                                          display_id.high, display_id.low);

  const auto& logical_origin = metrics.xdg_logical_origin;
  zaura_output_manager_v2_send_logical_position(
      resource(), output_name, logical_origin.x(), logical_origin.y());

  const auto& logical_size = metrics.xdg_logical_size;
  zaura_output_manager_v2_send_logical_size(
      resource(), output_name, logical_size.width(), logical_size.height());

  const auto& physical_size = metrics.wl_physical_size;
  zaura_output_manager_v2_send_physical_size(
      resource(), output_name, physical_size.width(), physical_size.height());

  const auto& insets = metrics.aura_logical_insets;
  zaura_output_manager_v2_send_work_area_insets(
      resource(), output_name, insets.top(), insets.left(), insets.bottom(),
      insets.right());

  zaura_output_manager_v2_send_device_scale_factor(
      resource(), output_name,
      base::bit_cast<uint32_t>(metrics.aura_device_scale_factor));

  zaura_output_manager_v2_send_logical_transform(
      resource(), output_name, metrics.aura_logical_transform);

  zaura_output_manager_v2_send_panel_transform(resource(), output_name,
                                               metrics.wl_panel_transform);

  if (send_done_on_config_change_) {
    SendDone();
  }
}

void TestZAuraOutputManagerV2::SendDone() {
  zaura_output_manager_v2_send_done(resource());
}

void TestZAuraOutputManagerV2::SendActivated(TestOutput* test_output) {
  zaura_output_manager_v2_send_activated(
      resource(),
      test_output->GetOutputName(wl_resource_get_client(resource())));
}

void TestZAuraOutputManagerV2::OnTestOutputGlobalDestroy(
    TestOutput* test_output) {
  if (send_done_on_config_change_) {
    SendDone();
  }
}

}  // namespace wl
