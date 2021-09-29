// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_BUTTON_TEST_API_H_
#define UI_VIEWS_TEST_BUTTON_TEST_API_H_

#include "base/macros.h"

namespace ui {
class Event;
}

namespace views {
class Button;

namespace test {

// A wrapper of Button to access private members for testing.
class ButtonTestApi {
 public:
  explicit ButtonTestApi(Button* button) : button_(button) {}

  void NotifyClick(const ui::Event& event);

 private:
  Button* button_;

  DISALLOW_COPY_AND_ASSIGN(ButtonTestApi);
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_BUTTON_TEST_API_H_
