// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_model_adapter.h"

#include <list>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view_class_properties.h"

namespace views {

MenuModelAdapter::MenuModelAdapter(
    ui::MenuModel* menu_model,
    base::RepeatingClosure on_menu_closed_callback)
    : menu_model_(menu_model),
      triggerable_event_flags_(ui::EF_LEFT_MOUSE_BUTTON |
                               ui::EF_RIGHT_MOUSE_BUTTON),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)) {
  CHECK(menu_model);
  // MenuModel does not allow changing from one non-null delegate to another.
  menu_model_->SetMenuModelDelegate(nullptr);
  menu_model_->SetMenuModelDelegate(this);
}

MenuModelAdapter::~MenuModelAdapter() {
  if (menu_model_) {
    menu_model_->SetMenuModelDelegate(nullptr);
  }
}

void MenuModelAdapter::BuildMenu(MenuItemView* menu) {
  DCHECK(menu);

  // Clear the menu.
  if (menu->HasSubmenu()) {
    menu->RemoveAllMenuItems();
  }

  // Leave entries in the map if the menu is being shown.  This
  // allows the map to find the menu model of submenus being closed
  // so ui::MenuModel::MenuClosed() can be called.
  if (!menu->GetMenuController()) {
    menu_map_.clear();
  }
  menu_map_[menu] = menu_model_;

  // Repopulate the menu.
  BuildMenuImpl(menu, menu_model_);
  menu->ChildrenChanged();
}

std::unique_ptr<MenuItemView> MenuModelAdapter::CreateMenu() {
  auto menu = std::make_unique<MenuItemView>(/*delegate=*/this);
  menu_ = menu.get();
  BuildMenu(menu.get());
  return menu;
}

std::optional<SkColor> MenuModelAdapter::GetLabelColor(int command_id) const {
  // Use STYLE_PRIMARY for title item. This aligns with 3-dot menu title style.
  return command_id == ui::MenuModel::kTitleId
             ? std::make_optional(
                   menu_->GetSubmenu()->GetColorProvider()->GetColor(
                       views::TypographyProvider::Get().GetColorId(
                           views::style::CONTEXT_MENU,
                           views::style::STYLE_PRIMARY)))
             : std::nullopt;
}

bool MenuModelAdapter::IsTearingDown() const {
  return !menu_model_;
}

// Static.
MenuItemView* MenuModelAdapter::AddMenuItemFromModelAt(ui::MenuModel* model,
                                                       size_t model_index,
                                                       MenuItemView* menu,
                                                       size_t menu_index,
                                                       int item_id) {
  std::optional<MenuItemView::Type> type;
  const auto menu_type = model->GetTypeAt(model_index);
  switch (menu_type) {
    case ui::MenuModel::TYPE_TITLE:
      type = MenuItemView::Type::kTitle;
      break;
    case ui::MenuModel::TYPE_COMMAND:
    case ui::MenuModel::TYPE_BUTTON_ITEM:
      type = MenuItemView::Type::kNormal;
      break;
    case ui::MenuModel::TYPE_CHECK:
      type = MenuItemView::Type::kCheckbox;
      break;
    case ui::MenuModel::TYPE_RADIO:
      type = MenuItemView::Type::kRadio;
      break;
    case ui::MenuModel::TYPE_SEPARATOR:
      type = MenuItemView::Type::kSeparator;
      break;
    case ui::MenuModel::TYPE_SUBMENU:
      type = MenuItemView::Type::kSubMenu;
      break;
    case ui::MenuModel::TYPE_ACTIONABLE_SUBMENU:
      type = MenuItemView::Type::kActionableSubMenu;
      break;
    case ui::MenuModel::TYPE_HIGHLIGHTED:
      type = MenuItemView::Type::kHighlighted;
      break;
  }

  if (*type == MenuItemView::Type::kSeparator) {
    return menu->AddMenuItemAt(menu_index, item_id, std::u16string(),
                               std::u16string(), std::u16string(),
                               ui::ImageModel(), ui::ImageModel(), *type,
                               model->GetSeparatorTypeAt(model_index));
  }

  const ui::ImageModel icon = model->GetIconAt(model_index);
  const ui::ImageModel minor_icon = model->GetMinorIconAt(model_index);
  auto* const menu_item_view = menu->AddMenuItemAt(
      menu_index, item_id, model->GetLabelAt(model_index),
      model->GetSecondaryLabelAt(model_index),
      model->GetMinorTextAt(model_index), minor_icon, icon, *type,
      ui::NORMAL_SEPARATOR, model->GetSubmenuBackgroundColorId(model_index),
      model->GetForegroundColorId(model_index),
      model->GetSelectedBackgroundColorId(model_index));

  if (model->IsAlertedAt(model_index)) {
    menu_item_view->SetAlerted();
  }
  menu_item_view->set_is_new(model->IsNewFeatureAt(model_index));
  menu_item_view->set_may_have_mnemonics(
      model->MayHaveMnemonicsAt(model_index));
  const ui::ElementIdentifier element_id =
      model->GetElementIdentifierAt(model_index);
  if (element_id) {
    menu_item_view->SetProperty(kElementIdentifierKey, element_id);
  }

  return menu_item_view;
}

// Static.
MenuItemView* MenuModelAdapter::AppendMenuItemFromModel(ui::MenuModel* model,
                                                        size_t model_index,
                                                        MenuItemView* menu,
                                                        int item_id) {
  const size_t menu_index =
      menu->HasSubmenu() ? menu->GetSubmenu()->children().size() : size_t{0};
  return AddMenuItemFromModelAt(model, model_index, menu, menu_index, item_id);
}

MenuItemView* MenuModelAdapter::AppendMenuItem(MenuItemView* menu,
                                               ui::MenuModel* model,
                                               size_t index) {
  return AppendMenuItemFromModel(model, index, menu,
                                 model->GetCommandIdAt(index));
}

// MenuModelAdapter, MenuDelegate implementation:

void MenuModelAdapter::ExecuteCommand(int id) {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  model->ActivatedAt(index);
}

void MenuModelAdapter::ExecuteCommand(int id, int mouse_event_flags) {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  model->ActivatedAt(index, mouse_event_flags);
}

bool MenuModelAdapter::IsTriggerableEvent(MenuItemView* source,
                                          const ui::Event& e) {
  return e.type() == ui::EventType::kGestureTap ||
         e.type() == ui::EventType::kGestureTapDown ||
         (e.IsMouseEvent() && (triggerable_event_flags_ & e.flags()));
}

bool MenuModelAdapter::GetAccelerator(int id,
                                      ui::Accelerator* accelerator) const {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  return model->GetAcceleratorAt(index, accelerator);
}

std::u16string MenuModelAdapter::GetLabel(int id) const {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  return model->GetLabelAt(index);
}

const gfx::FontList* MenuModelAdapter::GetLabelFontList(int id) const {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  if (ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index)) {
    if (const gfx::FontList* const font_list =
            model->GetLabelFontListAt(index)) {
      return font_list;
    }
  }

  // This line may be reached for the empty menu item.
  return MenuDelegate::GetLabelFontList(id);
}

bool MenuModelAdapter::IsCommandEnabled(int id) const {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  return model->IsEnabledAt(index);
}

bool MenuModelAdapter::IsCommandVisible(int id) const {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  return model->IsVisibleAt(index);
}

bool MenuModelAdapter::IsItemChecked(int id) const {
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  CHECK(ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index));
  return model->IsItemCheckedAt(index);
}

void MenuModelAdapter::WillShowMenu(MenuItemView* menu) {
  // Look up the menu model for this menu.
  const std::map<MenuItemView*,
                 raw_ptr<ui::MenuModel, CtnExperimental>>::const_iterator
      map_iterator = menu_map_.find(menu);
  CHECK(map_iterator != menu_map_.end());
  map_iterator->second->MenuWillShow();
}

void MenuModelAdapter::WillHideMenu(MenuItemView* menu) {
  // Look up the menu model for this menu.
  const std::map<MenuItemView*,
                 raw_ptr<ui::MenuModel, CtnExperimental>>::const_iterator
      map_iterator = menu_map_.find(menu);
  CHECK(map_iterator != menu_map_.end());
  map_iterator->second->MenuWillClose();
}

void MenuModelAdapter::OnMenuClosed(MenuItemView* menu) {
  if (!on_menu_closed_callback_.is_null()) {
    on_menu_closed_callback_.Run();
  }
}

// MenuModelDelegate overrides:
void MenuModelAdapter::OnMenuStructureChanged() {
  if (menu_) {
    BuildMenu(menu_);
  }
}

void MenuModelAdapter::OnMenuClearingDelegate() {
  menu_model_ = nullptr;
}

// MenuModelAdapter, private:

void MenuModelAdapter::BuildMenuImpl(MenuItemView* menu, ui::MenuModel* model) {
  CHECK(menu);
  CHECK(model);
  const size_t item_count = model->GetItemCount();
  for (size_t i = 0; i < item_count; ++i) {
    MenuItemView* const item = AppendMenuItem(menu, model, i);
    const auto type = model->GetTypeAt(i);
    const bool is_submenu = type == ui::MenuModel::TYPE_SUBMENU ||
                            type == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU;
    if (!item) {
      CHECK(!is_submenu);
      continue;
    }

    // Enabled state should be ignored for titles as they are non-interactive.
    item->SetEnabled(type != ui::MenuModel::TYPE_TITLE &&
                     model->IsEnabledAt(i));
    item->SetVisible(model->IsVisibleAt(i));

    if (is_submenu) {
      const auto item_type = item->GetType();
      CHECK(item_type == MenuItemView::Type::kSubMenu ||
            item_type == MenuItemView::Type::kActionableSubMenu);
      ui::MenuModel* const submodel = model->GetSubmenuModelAt(i);
      CHECK(submodel);
      BuildMenuImpl(item, submodel);
      menu_map_[item] = submodel;
    }
  }
}

}  // namespace views
