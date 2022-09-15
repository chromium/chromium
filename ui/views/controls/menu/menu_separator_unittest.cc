// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_separator.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using MenuSeparatorTest = ViewsTestBase;

TEST_F(MenuSeparatorTest, Metadata) {
  auto separator = std::make_unique<MenuSeparator>();
  test::TestViewMetadata(separator.get());
}

TEST_F(MenuSeparatorTest, TypeChangeEffect) {
  auto separator = std::make_unique<MenuSeparator>();
  separator->SizeToPreferredSize();
  const MenuConfig& config = MenuConfig::instance();
  EXPECT_EQ(config.separator_height, separator->height());

  separator->SetType(ui::MenuSeparatorType::DOUBLE_SEPARATOR);
  separator->SizeToPreferredSize();
  EXPECT_EQ(config.double_separator_height, separator->height());
}

}  // namespace views
