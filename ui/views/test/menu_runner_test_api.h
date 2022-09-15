// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MENU_RUNNER_TEST_API_H_
#define UI_VIEWS_TEST_MENU_RUNNER_TEST_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"

namespace views {

class MenuRunner;
class MenuRunnerHandler;

namespace test {

// A wrapper of MenuRunner to use testing methods of MenuRunner.
class MenuRunnerTestAPI {
 public:
  explicit MenuRunnerTestAPI(MenuRunner* menu_runner);

  MenuRunnerTestAPI(const MenuRunnerTestAPI&) = delete;
  MenuRunnerTestAPI& operator=(const MenuRunnerTestAPI&) = delete;

  ~MenuRunnerTestAPI();

  // Sets the menu runner handler.
  void SetMenuRunnerHandler(
      std::unique_ptr<MenuRunnerHandler> menu_runner_handler);

 private:
  raw_ptr<MenuRunner> menu_runner_;
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_MENU_RUNNER_TEST_API_H_
