// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_TEST_API_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_TEST_API_H_

#include "base/macros.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace ui {

// Test api class to access internals of |ui::TouchSelectionController| in
// tests.
class TouchSelectionControllerTestApi {
 public:
  explicit TouchSelectionControllerTestApi(
      TouchSelectionController* controller);
  ~TouchSelectionControllerTestApi();

  bool GetStartVisible() const;
  bool GetEndVisible() const;
  float GetStartAlpha() const;
  float GetEndAlpha() const;
  float GetInsertionHandleAlpha() const;
  TouchHandleOrientation GetStartHandleOrientation() const;
  TouchHandleOrientation GetEndHandleOrientation() const;

  bool temporarily_hidden() const { return controller_->temporarily_hidden_; }

 private:
  TouchSelectionController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerTestApi);
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_CONTROLLER_TEST_API_H_
