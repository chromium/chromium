// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_COMBOBOX_TEST_API_H_
#define UI_VIEWS_TEST_COMBOBOX_TEST_API_H_

#include "base/memory/raw_ptr.h"

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

  ComboboxTestApi(const ComboboxTestApi&) = delete;
  ComboboxTestApi& operator=(const ComboboxTestApi&) = delete;

  // Activates the Combobox menu item at |index|, as if selected by the user.
  void PerformActionAt(int index);

  // Installs a testing views::MenuRunnerHandler to test when a menu should be
  // shown. The test runner will immediately dismiss itself and increment the
  // given |menu_show_count| by one.
  void InstallTestMenuRunner(int* menu_show_count);

  // Accessors for private data members of Combobox.
  gfx::Size content_size();
  ui::MenuModel* menu_model();
  // Closes the menu of the combobox by calling private data members.
  void CloseMenu();

 private:
  const raw_ptr<Combobox> combobox_;
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_COMBOBOX_TEST_API_H_
