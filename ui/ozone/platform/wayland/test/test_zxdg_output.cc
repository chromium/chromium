// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zxdg_output.h"

#include <xdg-output-unstable-v1-server-protocol.h>

#include "ui/base/wayland/wayland_display_util.h"

namespace wl {

TestZXdgOutput::TestZXdgOutput(wl_resource* resource)
    : ServerObject(resource) {}

TestZXdgOutput::~TestZXdgOutput() = default;

void TestZXdgOutput::SetLogicalSize(const gfx::Size& size) {
  pending_logical_size_ = size;
}

void TestZXdgOutput::SendLogicalSize(const gfx::Size& size) {
  zxdg_output_v1_send_logical_size(resource(), size.width(), size.height());
}

void TestZXdgOutput::Flush() {
  if (pending_logical_size_) {
    logical_size_ = *pending_logical_size_;
    pending_logical_size_.reset();
    SendLogicalSize(*logical_size_);
  }
}

const struct zxdg_output_v1_interface kTestZXdgOutputImpl = {
    &DestroyResource,
};

}  // namespace wl
