// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/combobox_test_api.h"

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/test/menu_runner_test_api.h"

namespace views::test {
namespace {

// An dummy implementation of MenuRunnerHandler to check if the dropdown menu is
// shown or not.
class TestMenuRunnerHandler : public MenuRunnerHandler {
 public:
  explicit TestMenuRunnerHandler(int* show_counter)
      : show_counter_(show_counter) {}

  TestMenuRunnerHandler(const TestMenuRunnerHandler&) = delete;
  TestMenuRunnerHandler& operator=(const TestMenuRunnerHandler&) = delete;

  void RunMenuAt(Widget* parent,
                 MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 MenuAnchorPosition anchor,
                 ui::MenuSourceType source_type,
                 int32_t types) override {
    *show_counter_ += 1;
  }

 private:
  raw_ptr<int> show_counter_;
};

}  // namespace

void ComboboxTestApi::PerformActionAt(int index) {
  menu_model()->ActivatedAt(index);
}

void ComboboxTestApi::InstallTestMenuRunner(int* menu_show_count) {
  combobox_->menu_runner_ =
      std::make_unique<MenuRunner>(menu_model(), MenuRunner::COMBOBOX);
  test::MenuRunnerTestAPI test_api(combobox_->menu_runner_.get());
  test_api.SetMenuRunnerHandler(
      std::make_unique<TestMenuRunnerHandler>(menu_show_count));
}

gfx::Size ComboboxTestApi::content_size() {
  return combobox_->content_size_;
}

ui::MenuModel* ComboboxTestApi::menu_model() {
  return combobox_->menu_model_.get();
}

void ComboboxTestApi::CloseMenu() {
  return combobox_->OnMenuClosed(views::Button::ButtonState::STATE_NORMAL);
}

}  // namespace views::test
