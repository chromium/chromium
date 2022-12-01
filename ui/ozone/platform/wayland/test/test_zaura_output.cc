// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_output.h"

#include "ui/base/wayland/wayland_display_util.h"

namespace wl {
namespace {
int64_t display_id_counter = 10;
}

TestZAuraOutput::TestZAuraOutput(wl_resource* resource)
    : ServerObject(resource), display_id_(display_id_counter++) {
  if (wl_resource_get_version(resource) >=
      ZAURA_OUTPUT_DISPLAY_ID_SINCE_VERSION) {
    auto display_id = ui::wayland::ToWaylandDisplayIdPair(display_id_);
    zaura_output_send_display_id(resource, display_id.high, display_id.low);
  }
}

TestZAuraOutput::~TestZAuraOutput() = default;

void TestZAuraOutput::SendActivated() {
  zaura_output_send_activated(resource());
}

void TestZAuraOutput::Flush() {
  if (pending_insets_) {
    insets_ = std::move(*pending_insets_);
    pending_insets_.reset();
    zaura_output_send_insets(resource(), insets_.top(), insets_.left(),
                             insets_.bottom(), insets_.right());
  }
  if (pending_logical_transform_) {
    logical_transform_ = std::move(*pending_logical_transform_);
    pending_logical_transform_.reset();
    zaura_output_send_logical_transform(resource(), logical_transform_);
  }
}

const struct zaura_output_interface kTestZAuraOutputImpl {
  &DestroyResource,
};

}  // namespace wl
