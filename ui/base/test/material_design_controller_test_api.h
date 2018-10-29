// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_MATERIAL_DESIGN_CONTROLLER_TEST_API_H_
#define UI_BASE_TEST_MATERIAL_DESIGN_CONTROLLER_TEST_API_H_

#include "base/macros.h"
#include "ui/base/material_design/material_design_controller.h"

namespace ui {
namespace test {

// Test API to access the internal state of the MaterialDesignController class.
// Creating an instance of this class and then destroying it preserves global
// state in MaterialDesignController class.
class MaterialDesignControllerTestAPI {
 public:
  explicit MaterialDesignControllerTestAPI(bool touch_ui);
  ~MaterialDesignControllerTestAPI();

 private:
  const bool previous_touch_ui_;

  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTestAPI);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_MATERIAL_DESIGN_CONTROLLER_TEST_API_H_
