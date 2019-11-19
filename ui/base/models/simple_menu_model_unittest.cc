// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/simple_menu_model.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

namespace {

class DelegateBase : public SimpleMenuModel::Delegate {
 public:
  DelegateBase() : SimpleMenuModel::Delegate() {}

  ~DelegateBase() override = default;

  void set_icon_on_item(int command_id) { item_with_icon_ = command_id; }

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return true; }

  bool IsCommandIdEnabled(int command_id) const override {
    // Commands 0-99 are enabled.
    return command_id < 100;
  }

  bool IsCommandIdVisible(int command_id) const override {
    // Commands 0-99 are visible.
    return command_id < 100;
  }

  void ExecuteCommand(int command_id, int event_flags) override {}

  bool IsItemForCommandIdDynamic(int command_id) const override {
    return item_with_icon_ == command_id;
  }

  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override {
    return item_with_icon_ == command_id;
  }

 private:
  base::Optional<int> item_with_icon_;

  DISALLOW_COPY_AND_ASSIGN(DelegateBase);
};

TEST(SimpleMenuModelTest, SetLabel) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5,
                            base::ASCIIToUTF16("menu item 0"));

  simple_menu_model.SetLabel(/*index*/ 0, base::ASCIIToUTF16("new label"));

  ASSERT_EQ(base::ASCIIToUTF16("new label"), simple_menu_model.GetLabelAt(0));
}

TEST(SimpleMenuModelTest, SetEnabledAt) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5,
                            base::ASCIIToUTF16("menu item 0"));
  simple_menu_model.AddItem(/*command_id*/ 6,
                            base::ASCIIToUTF16("menu item 1"));

  simple_menu_model.SetEnabledAt(/*index*/ 0, false);
  simple_menu_model.SetEnabledAt(/*index*/ 1, true);

  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
  ASSERT_TRUE(simple_menu_model.IsEnabledAt(1));
}

TEST(SimpleMenuModelTest, SetVisibleAt) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5,
                            base::ASCIIToUTF16("menu item 0"));
  simple_menu_model.AddItem(/*command_id*/ 6,
                            base::ASCIIToUTF16("menu item 1"));

  simple_menu_model.SetVisibleAt(/*index*/ 0, false);
  simple_menu_model.SetVisibleAt(/*index*/ 1, true);

  ASSERT_FALSE(simple_menu_model.IsVisibleAt(0));
  ASSERT_TRUE(simple_menu_model.IsVisibleAt(1));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithNoDelegate) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, base::ASCIIToUTF16("menu item"));
  simple_menu_model.SetEnabledAt(/*index*/ 0, false);

  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithDelegateAndCommandEnabled) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 5 is enabled.
  simple_menu_model.AddItem(/*command_id*/ 5, base::ASCIIToUTF16("menu item"));
  simple_menu_model.SetEnabledAt(/*index*/ 0, true);

  // Should return false since the command_id 5 is enabled.
  ASSERT_TRUE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithDelegateAndCommandNotEnabled) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 108 is disabled.
  simple_menu_model.AddItem(/*command_id*/ 108,
                            base::ASCIIToUTF16("menu item"));
  simple_menu_model.SetEnabledAt(/*index*/ 0, true);

  // Should return false since the command_id 108 is disabled.
  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsVisibleAtWithDelegateAndCommandVisible) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 5 is visible.
  simple_menu_model.AddItem(/*command_id*/ 5, base::ASCIIToUTF16("menu item"));
  simple_menu_model.SetVisibleAt(/*index*/ 0, true);

  // Should return false since the command_id 5 is enabled.
  ASSERT_TRUE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsVisibleAtWithDelegateAndCommandNotVisible) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 108 is not visible.
  simple_menu_model.AddItem(/*command_id*/ 108,
                            base::ASCIIToUTF16("menu item"));
  simple_menu_model.SetVisibleAt(/*index*/ 0, true);

  // Should return false since the command_id 108 is not visible.
  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, HasIconsViaDelegate) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(/*command_id*/ 10, base::ASCIIToUTF16("menu item"));
  EXPECT_FALSE(simple_menu_model.HasIcons());

  simple_menu_model.AddItem(/*command_id*/ 11, base::ASCIIToUTF16("menu item"));
  delegate.set_icon_on_item(11);
  EXPECT_TRUE(simple_menu_model.HasIcons());
}

TEST(SimpleMenuModelTest, HasIconsViaAddItem) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(/*command_id*/ 10, base::ASCIIToUTF16("menu item"));
  EXPECT_FALSE(simple_menu_model.HasIcons());

  simple_menu_model.AddItemWithIcon(
      /*command_id*/ 11, base::ASCIIToUTF16("menu item"),
      gfx::test::CreateImage(16, 16).AsImageSkia());
  EXPECT_TRUE(simple_menu_model.HasIcons());
}

TEST(SimpleMenuModelTest, HasIconsViaVectorIcon) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(/*command_id*/ 10, base::ASCIIToUTF16("menu item"));
  EXPECT_FALSE(simple_menu_model.HasIcons());

  gfx::PathElement path[] = {gfx::CommandType::CIRCLE, 24, 18, 5};
  gfx::VectorIconRep rep[] = {{path, 4}};
  gfx::VectorIcon circle_icon = {rep, 1, "circle"};

  simple_menu_model.AddItemWithIcon(
      /*command_id*/ 11, base::ASCIIToUTF16("menu item"), circle_icon);
  EXPECT_TRUE(simple_menu_model.HasIcons());
}

}  // namespace

}  // namespace ui
