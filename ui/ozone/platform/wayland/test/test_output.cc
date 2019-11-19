// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_output.h"

#include <wayland-server-protocol-core.h>

namespace wl {

namespace {
constexpr uint32_t kOutputVersion = 2;
}

TestOutput::TestOutput()
    : GlobalObject(&wl_output_interface, nullptr, kOutputVersion) {}

TestOutput::~TestOutput() = default;

// Notify clients of the change for output position.
void TestOutput::OnBind() {
  if (rect_.IsEmpty())
    return;

  const char* kUnknownMake = "unknown";
  const char* kUnknownModel = "unknown";
  wl_output_send_geometry(resource(), rect_.x(), rect_.y(), 0, 0, 0,
                          kUnknownMake, kUnknownModel, 0);
  wl_output_send_mode(resource(), WL_OUTPUT_MODE_CURRENT, rect_.width(),
                      rect_.height(), 0);
  wl_output_send_done(resource());
}

void TestOutput::SetScale(int32_t factor) {
  wl_output_send_scale(resource(), factor);
  wl_output_send_done(resource());
}

}  // namespace wl
