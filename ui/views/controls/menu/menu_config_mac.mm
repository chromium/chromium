// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

namespace views {

void MenuConfig::InitPlatform() {
  check_selected_combobox_item = true;
  arrow_key_selection_wraps = false;
  use_mnemonics = false;
  show_context_menu_accelerators = false;
  all_menus_use_prefix_selection = true;
}

}  // namespace views
