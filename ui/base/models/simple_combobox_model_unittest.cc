// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/simple_combobox_model.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ui {

TEST(SimpleComboboxModelTest, StringItems) {
  std::vector<ui::SimpleComboboxModel::Item> items = {
      ui::SimpleComboboxModel::Item(u"item0"),
      ui::SimpleComboboxModel::Item::CreateSeparator(),
      ui::SimpleComboboxModel::Item(u"item2")};
  SimpleComboboxModel model(items);
  EXPECT_EQ(u"item0", model.GetItemAt(0));
  EXPECT_TRUE(model.IsItemSeparatorAt(1));
  EXPECT_EQ(u"item2", model.GetItemAt(2));

  // Update items.
  model.UpdateItemList({ui::SimpleComboboxModel::Item(u"item3"),
                        ui::SimpleComboboxModel::Item(u"item4")});
  EXPECT_EQ(u"item3", model.GetItemAt(0));
  EXPECT_EQ(u"item4", model.GetItemAt(1));
}

TEST(SimpleComboboxModelTest, ComboboxItems) {
  ui::SimpleComboboxModel::Item item16(
      u"Text16", u"SecondaryText16",
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16)));

  ui::SimpleComboboxModel::Item item20(
      u"Text20", u"SecondaryText20",
      ui::ImageModel::FromImage(gfx::test::CreateImage(20, 20)));

  SimpleComboboxModel model({item16, item20});
  EXPECT_EQ(u"Text16", model.GetItemAt(0));
  EXPECT_EQ(u"SecondaryText16", model.GetDropDownSecondaryTextAt(0));
  EXPECT_EQ(16, model.GetIconAt(0).Size().width());
  EXPECT_EQ(u"Text20", model.GetItemAt(1));
  EXPECT_EQ(u"SecondaryText20", model.GetDropDownSecondaryTextAt(1));
  EXPECT_EQ(20, model.GetIconAt(1).Size().width());
}

TEST(SimpleComboboxModelTest, GetDefaultIndex) {
  SimpleComboboxModel model{/*items=*/{}};
  EXPECT_EQ(std::nullopt, model.GetDefaultIndex());

  ui::SimpleComboboxModel::Item item(u"Text16");
  model.UpdateItemList(/*items=*/{item});
  EXPECT_EQ(0u, model.GetDefaultIndex());

  model.UpdateItemList(/*items=*/{});
  EXPECT_EQ(std::nullopt, model.GetDefaultIndex());
}

}  // namespace ui
