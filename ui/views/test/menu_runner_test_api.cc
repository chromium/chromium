// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/menu_runner_test_api.h"

#include <utility>

#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"

namespace views::test {

MenuRunnerTestAPI::MenuRunnerTestAPI(MenuRunner* menu_runner)
    : menu_runner_(menu_runner) {}

MenuRunnerTestAPI::~MenuRunnerTestAPI() = default;

void MenuRunnerTestAPI::SetMenuRunnerHandler(
    std::unique_ptr<MenuRunnerHandler> menu_runner_handler) {
  menu_runner_->SetRunnerHandler(std::move(menu_runner_handler));
}

}  // namespace views::test
