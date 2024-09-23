// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_BUTTON_TEST_API_H_
#define UI_VIEWS_TEST_BUTTON_TEST_API_H_

#include "base/memory/raw_ptr.h"

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

  ButtonTestApi(const ButtonTestApi&) = delete;
  ButtonTestApi& operator=(const ButtonTestApi&) = delete;

  void NotifyClick(const ui::Event& event);

 private:
  const raw_ptr<Button, DanglingUntriaged> button_;
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_BUTTON_TEST_API_H_
