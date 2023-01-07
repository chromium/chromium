// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/test_menu_item_view.h"

namespace views {

TestMenuItemView::TestMenuItemView() = default;

TestMenuItemView::TestMenuItemView(MenuDelegate* delegate)
    : MenuItemView(delegate) {}

TestMenuItemView::~TestMenuItemView() = default;

}  // namespace views
