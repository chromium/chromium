// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/material_design_controller_test_api.h"

namespace ui {
namespace test {

MaterialDesignControllerTestAPI::MaterialDesignControllerTestAPI(bool touch_ui)
    : previous_touch_ui_(MaterialDesignController::touch_ui_) {
  MaterialDesignController::SetTouchUi(touch_ui);
}

MaterialDesignControllerTestAPI::~MaterialDesignControllerTestAPI() {
  MaterialDesignController::touch_ui_ = previous_touch_ui_;
}

}  // namespace test
}  // namespace ui
