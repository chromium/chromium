// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_output.h"

#include <aura-shell-server-protocol.h>

namespace wl {

TestZAuraOutput::TestZAuraOutput(wl_resource* resource)
    : ServerObject(resource) {}

TestZAuraOutput::~TestZAuraOutput() = default;

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

}  // namespace wl
