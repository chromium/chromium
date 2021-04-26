// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MENU_RUNNER_TEST_API_H_
#define UI_VIEWS_TEST_MENU_RUNNER_TEST_API_H_

#include <memory>

#include "base/macros.h"

namespace views {

class MenuRunner;
class MenuRunnerHandler;

namespace test {

// A wrapper of MenuRunner to use testing methods of MenuRunner.
class MenuRunnerTestAPI {
 public:
  explicit MenuRunnerTestAPI(MenuRunner* menu_runner);
  ~MenuRunnerTestAPI();

  // Sets the menu runner handler.
  void SetMenuRunnerHandler(
      std::unique_ptr<MenuRunnerHandler> menu_runner_handler);

 private:
  MenuRunner* menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerTestAPI);
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_MENU_RUNNER_TEST_API_H_
