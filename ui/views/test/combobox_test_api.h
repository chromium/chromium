// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_COMBOBOX_TEST_API_H_
#define UI_VIEWS_TEST_COMBOBOX_TEST_API_H_

#include "base/macros.h"

namespace gfx {
class Size;
}

namespace ui {
class MenuModel;
}

namespace views {
class Combobox;

namespace test {

// A wrapper of Combobox to access private members for testing.
class ComboboxTestApi {
 public:
  explicit ComboboxTestApi(Combobox* combobox) : combobox_(combobox) {}

  // Activates the Combobox menu item at |index|, as if selected by the user.
  void PerformActionAt(int index);

  // Installs a testing views::MenuRunnerHandler to test when a menu should be
  // shown. The test runner will immediately dismiss itself and increment the
  // given |menu_show_count| by one.
  void InstallTestMenuRunner(int* menu_show_count);

  // Accessors for private data members of Combobox.
  gfx::Size content_size();
  ui::MenuModel* menu_model();

 private:
  Combobox* combobox_;

  DISALLOW_COPY_AND_ASSIGN(ComboboxTestApi);
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_COMBOBOX_TEST_API_H_
