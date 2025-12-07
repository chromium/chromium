// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_model_adapter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/views_test_base.h"

namespace {

// Base command id for test menu and its submenu.
constexpr int kRootIdBase = 100;
constexpr int kSubmenuIdBase = 200;
constexpr int kActionableSubmenuIdBase = 300;

class MenuModelBase : public ui::MenuModel {
 public:
  explicit MenuModelBase(int command_id_base)
      : command_id_base_(command_id_base) {}

  MenuModelBase(const MenuModelBase&) = delete;
  MenuModelBase& operator=(const MenuModelBase&) = delete;

  ~MenuModelBase() override = default;

  // ui::MenuModel implementation:
  size_t GetItemCount() const override { return items_.size(); }

  ItemType GetTypeAt(size_t index) const override { return items_[index].type; }

  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(size_t index) const override {
    return static_cast<int>(index) + command_id_base_;
  }

  std::u16string GetLabelAt(size_t index) const override {
    return items_[index].label;
  }

  bool IsItemDynamicAt(size_t index) const override { return false; }

  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(size_t index) const override { return false; }

  int GetGroupIdAt(size_t index) const override { return 0; }

  ui::ImageModel GetIconAt(size_t index) const override {
    return ui::ImageModel();
  }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override {
    return nullptr;
  }

  bool IsEnabledAt(size_t index) const override {
    return items_[index].enabled;
  }

  bool IsVisibleAt(size_t index) const override {
    return items_[index].visible;
  }

  bool IsAlertedAt(size_t index) const override {
    return items_[index].alerted;
  }

  bool IsNewFeatureAt(size_t index) const override {
    return items_[index].new_feature;
  }

  MenuModel* GetSubmenuModelAt(size_t index) const override {
    return items_[index].submenu;
  }

  void ActivatedAt(size_t index) override { set_last_activation(index); }

  void ActivatedAt(size_t index, int event_flags) override {
    ActivatedAt(index);
  }

  void MenuWillShow() override {}

  void MenuWillClose() override {}

  // Item definition.
  struct Item {
    Item(ItemType item_type,
         const std::string& item_label,
         ui::MenuModel* item_submenu)
        : type(item_type),
          label(base::ASCIIToUTF16(item_label)),
          submenu(item_submenu) {}

    Item(ItemType item_type,
         const std::string& item_label,
         ui::MenuModel* item_submenu,
         bool enabled,
         bool visible)
        : type(item_type),
          label(base::ASCIIToUTF16(item_label)),
          submenu(item_submenu),
          enabled(enabled),
          visible(visible) {}

    ItemType type;
    std::u16string label;
    raw_ptr<ui::MenuModel> submenu;
    bool enabled = true;
    bool visible = true;
    bool alerted = false;
    bool new_feature = false;
  };

  const Item& GetItemDefinition(size_t index) { return items_[index]; }

  // Access index argument to ActivatedAt().
  std::optional<size_t> last_activation() const { return last_activation_; }
  void set_last_activation(std::optional<size_t> last_activation) {
    last_activation_ = last_activation;
  }

 protected:
  std::vector<Item> items_;

 private:
  int command_id_base_;
  std::optional<size_t> last_activation_;
};

class SubmenuModel final : public MenuModelBase {
 public:
  SubmenuModel() : MenuModelBase(kSubmenuIdBase) {
    items_.emplace_back(TYPE_COMMAND, "submenu item 0", nullptr, false, true);
    items_.emplace_back(TYPE_COMMAND, "submenu item 1", nullptr);
    items_[1].alerted = true;
  }

  SubmenuModel(const SubmenuModel&) = delete;
  SubmenuModel& operator=(const SubmenuModel&) = delete;

  ~SubmenuModel() override = default;

  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<SubmenuModel> weak_ptr_factory_{this};
};

class ActionableSubmenuModel final : public MenuModelBase {
 public:
  ActionableSubmenuModel() : MenuModelBase(kActionableSubmenuIdBase) {
    items_.emplace_back(TYPE_COMMAND, "actionable submenu item 0", nullptr);
    items_.emplace_back(TYPE_COMMAND, "actionable submenu item 1", nullptr);
    items_[1].new_feature = true;
  }

  ActionableSubmenuModel(const ActionableSubmenuModel&) = delete;
  ActionableSubmenuModel& operator=(const ActionableSubmenuModel&) = delete;

  ~ActionableSubmenuModel() override = default;

  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<ActionableSubmenuModel> weak_ptr_factory_{this};
};

class RootModel final : public MenuModelBase {
 public:
  RootModel() : MenuModelBase(kRootIdBase) {
    submenu_model_ = std::make_unique<SubmenuModel>();
    actionable_submenu_model_ = std::make_unique<ActionableSubmenuModel>();

    items_.emplace_back(TYPE_COMMAND, "command 0", nullptr, false, false);
    items_.emplace_back(TYPE_CHECK, "check 1", nullptr);
    items_.emplace_back(TYPE_SEPARATOR, "", nullptr);
    items_.emplace_back(TYPE_SUBMENU, "submenu 3", submenu_model_.get());
    items_.emplace_back(TYPE_RADIO, "radio 4", nullptr);
    items_.emplace_back(TYPE_ACTIONABLE_SUBMENU, "actionable 5",
                        actionable_submenu_model_.get());
  }

  RootModel(const RootModel&) = delete;
  RootModel& operator=(const RootModel&) = delete;

  ~RootModel() override {
    // Avoid that the pointer to `submenu_model_` becomes dangling.
    items_.clear();
  }

  base::WeakPtr<ui::MenuModel> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<MenuModel> submenu_model_;
  std::unique_ptr<MenuModel> actionable_submenu_model_;
  base::WeakPtrFactory<RootModel> weak_ptr_factory_{this};
};

void CheckSubmenu(const RootModel& model,
                  views::MenuItemView* menu,
                  views::MenuModelAdapter* delegate,
                  int submenu_id,
                  size_t expected_children,
                  size_t submenu_model_index,
                  int id) {
  views::MenuItemView* submenu = menu->GetMenuItemByID(submenu_id);
  views::SubmenuView* subitem_container = submenu->GetSubmenu();
  EXPECT_EQ(expected_children, subitem_container->children().size());

  MenuModelBase* submodel =
      static_cast<MenuModelBase*>(model.GetSubmenuModelAt(submenu_model_index));
  EXPECT_TRUE(submodel);
  for (size_t i = 0; i < subitem_container->children().size(); ++i, ++id) {
    const MenuModelBase::Item& model_item = submodel->GetItemDefinition(i);

    views::MenuItemView* item = menu->GetMenuItemByID(id);
    if (!item) {
      EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model_item.type);
      continue;
    }
    // Check placement.
    EXPECT_EQ(i, submenu->GetSubmenu()->GetIndexOf(item));

    // Check type.
    switch (model_item.type) {
      case ui::MenuModel::TYPE_TITLE:
        EXPECT_EQ(views::MenuItemView::Type::kTitle, item->GetType());
        break;
      case ui::MenuModel::TYPE_COMMAND:
        EXPECT_EQ(views::MenuItemView::Type::kNormal, item->GetType());
        break;
      case ui::MenuModel::TYPE_CHECK:
        EXPECT_EQ(views::MenuItemView::Type::kCheckbox, item->GetType());
        break;
      case ui::MenuModel::TYPE_RADIO:
        EXPECT_EQ(views::MenuItemView::Type::kRadio, item->GetType());
        break;
      case ui::MenuModel::TYPE_SEPARATOR:
      case ui::MenuModel::TYPE_BUTTON_ITEM:
        break;
      case ui::MenuModel::TYPE_SUBMENU:
        EXPECT_EQ(views::MenuItemView::Type::kSubMenu, item->GetType());
        break;
      case ui::MenuModel::TYPE_ACTIONABLE_SUBMENU:
        EXPECT_EQ(views::MenuItemView::Type::kActionableSubMenu,
                  item->GetType());
        break;
      case ui::MenuModel::TYPE_HIGHLIGHTED:
        EXPECT_EQ(views::MenuItemView::Type::kHighlighted, item->GetType());
        break;
    }

    // Check enabled state.
    EXPECT_EQ(model_item.enabled, item->GetEnabled());

    // Check visibility.
    EXPECT_EQ(model_item.visible, item->GetVisible());

    // Check alert state.
    EXPECT_EQ(model_item.alerted, item->is_alerted());

    // Check new feature flag.
    EXPECT_EQ(model_item.new_feature, item->is_new());

    // Check activation.
    static_cast<views::MenuDelegate*>(delegate)->ExecuteCommand(id);
    EXPECT_EQ(i, submodel->last_activation());
    submodel->set_last_activation(std::nullopt);
  }
}

}  // namespace

namespace views {

using MenuModelAdapterTest = ViewsTestBase;

TEST_F(MenuModelAdapterTest, BasicTest) {
  // Build model and adapter.
  RootModel model;
  views::MenuModelAdapter delegate(&model);

  // Create menu.  Build menu twice to check that rebuilding works properly.
  auto menu_owning = std::make_unique<MenuItemView>(&delegate);
  MenuItemView* menu = menu_owning.get();
  MenuRunner menu_runner(std::move(menu_owning), 0);
  delegate.BuildMenu(menu);
  delegate.BuildMenu(menu);
  EXPECT_TRUE(menu->HasSubmenu());

  // Check top level menu items.
  views::SubmenuView* item_container = menu->GetSubmenu();
  EXPECT_EQ(6u, item_container->children().size());

  int id = kRootIdBase;
  for (size_t i = 0; i < item_container->children().size(); ++i, ++id) {
    const MenuModelBase::Item& model_item = model.GetItemDefinition(i);

    MenuItemView* item = menu->GetMenuItemByID(id);
    if (!item) {
      EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, model_item.type);
      continue;
    }

    // Check placement.
    EXPECT_EQ(i, menu->GetSubmenu()->GetIndexOf(item));

    // Check type.
    switch (model_item.type) {
      case ui::MenuModel::TYPE_TITLE:
        EXPECT_EQ(views::MenuItemView::Type::kTitle, item->GetType());
        break;
      case ui::MenuModel::TYPE_COMMAND:
        EXPECT_EQ(views::MenuItemView::Type::kNormal, item->GetType());
        break;
      case ui::MenuModel::TYPE_CHECK:
        EXPECT_EQ(views::MenuItemView::Type::kCheckbox, item->GetType());
        break;
      case ui::MenuModel::TYPE_RADIO:
        EXPECT_EQ(views::MenuItemView::Type::kRadio, item->GetType());
        break;
      case ui::MenuModel::TYPE_SEPARATOR:
      case ui::MenuModel::TYPE_BUTTON_ITEM:
        break;
      case ui::MenuModel::TYPE_SUBMENU:
        EXPECT_EQ(views::MenuItemView::Type::kSubMenu, item->GetType());
        break;
      case ui::MenuModel::TYPE_ACTIONABLE_SUBMENU:
        EXPECT_EQ(views::MenuItemView::Type::kActionableSubMenu,
                  item->GetType());
        break;
      case ui::MenuModel::TYPE_HIGHLIGHTED:
        EXPECT_EQ(views::MenuItemView::Type::kHighlighted, item->GetType());
        break;
    }

    // Check enabled state.
    EXPECT_EQ(model_item.enabled, item->GetEnabled());

    // Check visibility.
    EXPECT_EQ(model_item.visible, item->GetVisible());

    // Check alert state.
    EXPECT_EQ(model_item.alerted, item->is_alerted());

    // Check new feature flag.
    EXPECT_EQ(model_item.new_feature, item->is_new());

    // Check activation.
    static_cast<views::MenuDelegate*>(&delegate)->ExecuteCommand(id);
    EXPECT_EQ(i, model.last_activation());
    model.set_last_activation(std::nullopt);
  }

  // Check the submenu.
  const int submenu_index = 3;
  CheckSubmenu(model, menu, &delegate, kRootIdBase + submenu_index, 2,
               submenu_index, kSubmenuIdBase);

  // Check the actionable submenu.
  const int actionable_submenu_index = 5;
  CheckSubmenu(model, menu, &delegate, kRootIdBase + actionable_submenu_index,
               2, actionable_submenu_index, kActionableSubmenuIdBase);
}

}  // namespace views
