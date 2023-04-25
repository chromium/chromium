// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_output.h"

#include "base/bit_cast.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/ozone/platform/wayland/test/test_output_metrics.h"

namespace wl {

TestZAuraOutput::TestZAuraOutput(wl_resource* resource)
    : ServerObject(resource) {}

TestZAuraOutput::~TestZAuraOutput() = default;

void TestZAuraOutput::SendActivated() {
  zaura_output_send_activated(resource());
}

void TestZAuraOutput::Flush(const TestOutputMetrics& metrics) {
  if (wl_resource_get_version(resource()) >=
      ZAURA_OUTPUT_DISPLAY_ID_SINCE_VERSION) {
    auto display_id =
        ui::wayland::ToWaylandDisplayIdPair(metrics.aura_display_id);
    zaura_output_send_display_id(resource(), display_id.high, display_id.low);
  }

  const auto insets = metrics.aura_logical_insets;
  zaura_output_send_insets(resource(), insets.top(), insets.left(),
                           insets.bottom(), insets.right());

  zaura_output_send_device_scale_factor(
      resource(), base::bit_cast<uint32_t>(metrics.aura_device_scale_factor));

  zaura_output_send_logical_transform(resource(),
                                      metrics.aura_logical_transform);
}

const struct zaura_output_interface kTestZAuraOutputImpl {
  &DestroyResource,
};

}  // namespace wl
