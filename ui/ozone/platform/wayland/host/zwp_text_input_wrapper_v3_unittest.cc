// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v3.h"

#include <memory>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/test_zwp_text_input_wrapper_client.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {

class ZWPTextInputWrapperV3Test : public WaylandTestSimple {
 public:
  ZWPTextInputWrapperV3Test()
      : WaylandTestSimple(
            {.text_input_wrapper_type = ui::ZWPTextInputWrapperType::kV3}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();

    wrapper_ = std::make_unique<ZWPTextInputWrapperV3>(
        connection_.get(), &test_client_, connection_->text_input_manager_v3());
  }

 protected:
  TestZWPTextInputWrapperClient test_client_;
  std::unique_ptr<ZWPTextInputWrapperV3> wrapper_;
};

}  // namespace ui
