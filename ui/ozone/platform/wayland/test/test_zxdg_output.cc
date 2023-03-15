// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zxdg_output.h"

#include <xdg-output-unstable-v1-server-protocol.h>

#include "ui/base/wayland/wayland_display_util.h"
#include "ui/ozone/platform/wayland/test/test_output_metrics.h"

namespace wl {

TestZXdgOutput::TestZXdgOutput(wl_resource* resource)
    : ServerObject(resource) {}

TestZXdgOutput::~TestZXdgOutput() = default;

void TestZXdgOutput::Flush(const TestOutputMetrics& metrics) {
  zxdg_output_v1_send_logical_size(resource(), metrics.xdg_logical_size.width(),
                                   metrics.xdg_logical_size.height());
  zxdg_output_v1_send_logical_position(resource(),
                                       metrics.xdg_logical_origin.x(),
                                       metrics.xdg_logical_origin.y());
}

const struct zxdg_output_v1_interface kTestZXdgOutputImpl = {
    &DestroyResource,
};

}  // namespace wl
