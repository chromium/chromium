// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/simple_menu_model.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

namespace {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementID);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementID);

constexpr int kAlertedCommandId = 2;

class DelegateBase : public SimpleMenuModel::Delegate {
 public:
  DelegateBase() : SimpleMenuModel::Delegate() {}

  DelegateBase(const DelegateBase&) = delete;
  DelegateBase& operator=(const DelegateBase&) = delete;

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

  bool IsCommandIdAlerted(int command_id) const override {
    return command_id == kAlertedCommandId;
  }

  void ExecuteCommand(int command_id, int event_flags) override {}

  bool IsItemForCommandIdDynamic(int command_id) const override {
    return item_with_icon_ == command_id;
  }

  ImageModel GetIconForCommandId(int command_id) const override {
    return item_with_icon_ == command_id
               ? ImageModel::FromImage(gfx::test::CreateImage(16, 16))
               : ImageModel();
  }

 private:
  std::optional<int> item_with_icon_;
};

class MockDelegate : public DelegateBase {
 public:
  MOCK_METHOD(bool, IsCommandIdEnabled, (int command_id), (const override));
};

TEST(SimpleMenuModelTest, AddSeparatorPreventsEmptySections) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddSeparator(ui::NORMAL_SEPARATOR);

  // Should return 0 since no item is present yet to be separated.
  ASSERT_EQ(0u, simple_menu_model.GetItemCount());

  simple_menu_model.AddItem(/*command_id*/ 1, u"menu item");
  simple_menu_model.SetVisibleAt(/*index*/ 0, false);
  simple_menu_model.AddSeparator(ui::NORMAL_SEPARATOR);

  // Should return 1 since an invisible item doesn't need to be separated.
  ASSERT_EQ(1u, simple_menu_model.GetItemCount());

  simple_menu_model.SetVisibleAt(/*index*/ 0, true);
  simple_menu_model.AddSeparator(ui::NORMAL_SEPARATOR);

  // Should return 2 since a visible item should be separated.
  ASSERT_EQ(2u, simple_menu_model.GetItemCount());

  simple_menu_model.AddSeparator(ui::NORMAL_SEPARATOR);

  // Should return 2 since a separator shouldn't directly precede another one.
  ASSERT_EQ(2u, simple_menu_model.GetItemCount());
}

TEST(SimpleMenuModelTest, SetLabel) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item 0");

  simple_menu_model.SetLabel(/*index*/ 0, u"new label");

  ASSERT_EQ(u"new label", simple_menu_model.GetLabelAt(0));
}

TEST(SimpleMenuModelTest, SetEnabledAt) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item 0");
  simple_menu_model.AddItem(/*command_id*/ 6, u"menu item 1");

  simple_menu_model.SetEnabledAt(/*index*/ 0, false);
  simple_menu_model.SetEnabledAt(/*index*/ 1, true);

  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
  ASSERT_TRUE(simple_menu_model.IsEnabledAt(1));
}

TEST(SimpleMenuModelTest, SetVisibleAt) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item 0");
  simple_menu_model.AddItem(/*command_id*/ 6, u"menu item 1");

  simple_menu_model.SetVisibleAt(/*index*/ 0, false);
  simple_menu_model.SetVisibleAt(/*index*/ 1, true);

  ASSERT_FALSE(simple_menu_model.IsVisibleAt(0));
  ASSERT_TRUE(simple_menu_model.IsVisibleAt(1));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithNoDelegate) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item");
  simple_menu_model.SetEnabledAt(/*index*/ 0, false);

  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithDelegateAndCommandEnabled) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 5 is enabled.
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item");
  simple_menu_model.SetEnabledAt(/*index*/ 0, true);

  // Should return false since the command_id 5 is enabled.
  ASSERT_TRUE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithDelegateAndCommandNotEnabled) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 108 is disabled.
  simple_menu_model.AddItem(/*command_id*/ 108, u"menu item");
  simple_menu_model.SetEnabledAt(/*index*/ 0, true);

  // Should return false since the command_id 108 is disabled.
  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsEnabledAtWithDelegateTitle) {
  MockDelegate delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddTitle(u"title");

  // Expect that for title elements the `delegate` is not queried. They are
  // always considered disabled.
  EXPECT_CALL(delegate, IsCommandIdEnabled).Times(0);
  ASSERT_FALSE(simple_menu_model.IsEnabledAt(0));
}

TEST(SimpleMenuModelTest, IsVisibleAtWithDelegateAndCommandVisible) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 5 is visible.
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item");
  simple_menu_model.SetVisibleAt(/*index*/ 0, true);

  // Should return true since the command_id 5 is visible.
  ASSERT_TRUE(simple_menu_model.IsVisibleAt(0));
}

TEST(SimpleMenuModelTest, IsVisibleAtWithDelegateAndCommandNotVisible) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  // CommandId 108 is not visible.
  simple_menu_model.AddItem(/*command_id*/ 108, u"menu item");
  simple_menu_model.SetVisibleAt(/*index*/ 0, true);

  // Should return false since the command_id 108 is not visible.
  ASSERT_FALSE(simple_menu_model.IsVisibleAt(0));
}

TEST(SimpleMenuModelTest, IsAlertedAtViaDelegate) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(kAlertedCommandId, u"alerted item");
  simple_menu_model.AddItem(kAlertedCommandId + 1, u"non-alerted item");

  EXPECT_TRUE(simple_menu_model.IsAlertedAt(0));
  EXPECT_FALSE(simple_menu_model.IsAlertedAt(1));
}

TEST(SimpleMenuModelTest, SetIsNewFeatureAt) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item 0");
  simple_menu_model.AddItem(/*command_id*/ 6, u"menu item 1");

  simple_menu_model.SetIsNewFeatureAt(
      /*index*/ 0, IsNewFeatureAtValue::create_for_test(false));
  simple_menu_model.SetIsNewFeatureAt(
      /*index*/ 1, IsNewFeatureAtValue::create_for_test(true));

  ASSERT_FALSE(simple_menu_model.IsNewFeatureAt(0));
  ASSERT_TRUE(simple_menu_model.IsNewFeatureAt(1));
}

TEST(SimpleMenuModelTest, SetElementIdentifierAt) {
  SimpleMenuModel simple_menu_model(nullptr);
  simple_menu_model.AddItem(/*command_id*/ 5, u"menu item 0");
  simple_menu_model.AddItem(/*command_id*/ 6, u"menu item 1");

  simple_menu_model.SetElementIdentifierAt(/*index*/ 1, kTestElementID);

  EXPECT_EQ(ui::ElementIdentifier(),
            simple_menu_model.GetElementIdentifierAt(0));
  EXPECT_EQ(kTestElementID, simple_menu_model.GetElementIdentifierAt(1));
}

TEST(SimpleMenuModelTest, HasIconsViaDelegate) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(/*command_id*/ 10, u"menu item");
  EXPECT_TRUE(simple_menu_model.GetIconAt(0).IsEmpty());

  simple_menu_model.AddItem(/*command_id*/ 11, u"menu item");
  delegate.set_icon_on_item(11);
  EXPECT_FALSE(simple_menu_model.GetIconAt(1).IsEmpty());
}

TEST(SimpleMenuModelTest, HasIconsViaAddItem) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(/*command_id*/ 10, u"menu item");
  EXPECT_TRUE(simple_menu_model.GetIconAt(0).IsEmpty());

  simple_menu_model.AddItemWithIcon(
      /*command_id*/ 11, u"menu item",
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16)));
  EXPECT_FALSE(simple_menu_model.GetIconAt(1).IsEmpty());
}

TEST(SimpleMenuModelTest, HasIconsViaVectorIcon) {
  DelegateBase delegate;
  SimpleMenuModel simple_menu_model(&delegate);
  simple_menu_model.AddItem(/*command_id*/ 10, u"menu item");
  EXPECT_TRUE(simple_menu_model.GetIconAt(0).IsEmpty());

  gfx::PathElement path[] = {gfx::CommandType::CIRCLE, 24, 18, 5};
  gfx::VectorIconRep rep[] = {{path, 4}};
  gfx::VectorIcon circle_icon = {rep, 1, "circle"};

  simple_menu_model.AddItemWithIcon(
      /*command_id*/ 11, u"menu item",
      ui::ImageModel::FromVectorIcon(circle_icon, ui::kColorMenuIcon, 16));
  EXPECT_FALSE(simple_menu_model.GetIconAt(1).IsEmpty());
}

TEST(SimpleMenuModelTest, InheritsSubMenuAlert) {
  DelegateBase delegate;
  SimpleMenuModel submenu_model(&delegate);
  submenu_model.AddItem(kAlertedCommandId + 1, u"menu item");

  // The alerted menu item is not present in the submenu.
  SimpleMenuModel parent_menu_model(&delegate);
  parent_menu_model.AddSubMenu(/*command_id*/ 10, u"submenu", &submenu_model);
  EXPECT_FALSE(parent_menu_model.IsAlertedAt(0));

  // Add the alerted menu item to the submenu. Now both the submenu item and
  // the item in the submenu should show as alerted.
  submenu_model.AddItem(kAlertedCommandId, u"alerted item");
  EXPECT_TRUE(submenu_model.IsAlertedAt(1));
  EXPECT_TRUE(parent_menu_model.IsAlertedAt(0));
}

}  // namespace

}  // namespace ui
