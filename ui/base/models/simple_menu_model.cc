// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/simple_menu_model.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"

namespace ui {

const int kSeparatorId = -1;

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel::Delegate, public:

bool SimpleMenuModel::Delegate::IsCommandIdChecked(int command_id) const {
  return false;
}

bool SimpleMenuModel::Delegate::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool SimpleMenuModel::Delegate::IsCommandIdVisible(int command_id) const {
  return true;
}

bool SimpleMenuModel::Delegate::IsCommandIdAlerted(int command_id) const {
  return false;
}

bool SimpleMenuModel::Delegate::IsElementIdAlerted(
    ui::ElementIdentifier element_id) const {
  return false;
}

bool SimpleMenuModel::Delegate::IsItemForCommandIdDynamic(
    int command_id) const {
  return false;
}

std::u16string SimpleMenuModel::Delegate::GetLabelForCommandId(
    int command_id) const {
  return std::u16string();
}

ImageModel SimpleMenuModel::Delegate::GetIconForCommandId(
    int command_id) const {
  return ImageModel();
}

void SimpleMenuModel::Delegate::OnMenuWillShow(SimpleMenuModel* /*source*/) {}

void SimpleMenuModel::Delegate::MenuClosed(SimpleMenuModel* /*source*/) {
}

bool SimpleMenuModel::Delegate::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, public:

SimpleMenuModel::SimpleMenuModel(Delegate* delegate) : delegate_(delegate) {}

SimpleMenuModel::~SimpleMenuModel() = default;

void SimpleMenuModel::AddItem(int command_id, const std::u16string& label) {
  AppendItem(Item(command_id, TYPE_COMMAND, label));
}

void SimpleMenuModel::AddItemWithStringId(int command_id, int string_id) {
  // Prevent this dangerous pattern:
  //   model->AddItemWithStringId(IDS_FOO, IDS_FOO);
  // This conflates string IDs with command IDs, which are separate namespaces.
  // Sometimes this is an accident where this is meant:
  //   model->AddItemWithStringId(IDC_FOO, IDS_FOO);
  // but sometimes it is deliberate, usually in situations where there is no
  // matching IDC constant or the matching IDC constant is not available.
  // Using IDS constants for command IDs can cause confusion elsewhere, since
  // command IDs are usually either IDC values or strictly local constants.
  DCHECK_NE(command_id, string_id);
  AddItem(command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddItemWithIcon(int command_id,
                                      const std::u16string& label,
                                      const ImageModel& icon) {
  Item item(command_id, TYPE_COMMAND, label);
  item.icon = icon;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddItemWithStringIdAndIcon(int command_id,
                                                 int string_id,
                                                 const ImageModel& icon) {
  AddItemWithIcon(command_id, l10n_util::GetStringUTF16(string_id), icon);
}

void SimpleMenuModel::AddCheckItem(int command_id,
                                   const std::u16string& label) {
  AppendItem(Item(command_id, TYPE_CHECK, label));
}

void SimpleMenuModel::AddCheckItemWithStringId(int command_id, int string_id) {
  AddCheckItem(command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddRadioItem(int command_id,
                                   const std::u16string& label,
                                   int group_id) {
  Item item(command_id, TYPE_RADIO, label);
  item.group_id = group_id;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddRadioItemWithStringId(int command_id, int string_id,
                                               int group_id) {
  AddRadioItem(command_id, l10n_util::GetStringUTF16(string_id), group_id);
}

void SimpleMenuModel::AddHighlightedItemWithIcon(int command_id,
                                                 const std::u16string& label,
                                                 const ImageModel& icon) {
  Item item(command_id, TYPE_HIGHLIGHTED, label);
  item.icon = icon;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddTitle(const std::u16string& label) {
  Item item(kTitleId, TYPE_TITLE, label);
  // Titles are non-interactive and should not be enabled.
  item.enabled = false;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddTitleWithStringId(int string_id) {
  AddTitle(l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddSeparator(MenuSeparatorType separator_type) {
  if (items_.empty()) {
    if (separator_type == NORMAL_SEPARATOR) {
      return;
    }
    DCHECK_EQ(SPACING_SEPARATOR, separator_type);
  } else {
    size_t last_visible_item = items_.size();
    for (auto i = items_.size(); i > 0; i--) {
      if (IsVisibleAt(i - 1)) {
        last_visible_item = i - 1;
        break;
      }
    }

    if (last_visible_item == items_.size()) {
      // No visible items. Don't add a separator.
      return;
    }

    if (items_.at(last_visible_item).type == TYPE_SEPARATOR) {
      DCHECK_EQ(NORMAL_SEPARATOR, separator_type);
      DCHECK_EQ(NORMAL_SEPARATOR, items_.at(last_visible_item).separator_type);
      // The last item is already a separator. Don't add another.
      return;
    }
  }
#if !defined(USE_AURA)
  if (separator_type == SPACING_SEPARATOR)
    NOTIMPLEMENTED();
#endif
  Item item(kSeparatorId, TYPE_SEPARATOR, std::u16string());
  item.separator_type = separator_type;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddButtonItem(int command_id,
                                    ButtonMenuItemModel* model) {
  Item item(command_id, TYPE_BUTTON_ITEM, std::u16string());
  item.button_model = model;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddSubMenu(int command_id,
                                 const std::u16string& label,
                                 MenuModel* model) {
  Item item(command_id, TYPE_SUBMENU, label);
  item.submenu = model;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddSubMenuWithStringId(int command_id,
                                             int string_id, MenuModel* model) {
  AddSubMenu(command_id, l10n_util::GetStringUTF16(string_id), model);
}

void SimpleMenuModel::AddSubMenuWithIcon(int command_id,
                                         const std::u16string& label,
                                         MenuModel* model,
                                         const ImageModel& icon) {
  Item item(command_id, TYPE_SUBMENU, label);
  item.submenu = model;
  item.icon = icon;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddSubMenuWithStringIdAndIcon(int command_id,
                                                    int string_id,
                                                    MenuModel* model,
                                                    const ImageModel& icon) {
  Item item(command_id, TYPE_SUBMENU, l10n_util::GetStringUTF16(string_id));
  item.submenu = model;
  item.icon = icon;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddActionableSubMenu(int command_id,
                                           const std::u16string& label,
                                           MenuModel* model) {
  Item item(command_id, TYPE_ACTIONABLE_SUBMENU, label);
  item.submenu = model;
  AppendItem(std::move(item));
}

void SimpleMenuModel::AddActionableSubmenuWithStringIdAndIcon(
    int command_id,
    int string_id,
    MenuModel* model,
    const ImageModel& icon) {
  Item item(command_id, TYPE_ACTIONABLE_SUBMENU,
            l10n_util::GetStringUTF16(string_id));
  item.submenu = model;
  item.icon = icon;
  AppendItem(std::move(item));
}

void SimpleMenuModel::InsertItemAt(size_t index,
                                   int command_id,
                                   const std::u16string& label) {
  InsertItemAtIndex(Item(command_id, TYPE_COMMAND, label), index);
}

void SimpleMenuModel::InsertItemWithStringIdAt(size_t index,
                                               int command_id,
                                               int string_id) {
  InsertItemAt(index, command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::InsertCheckItemAt(size_t index,
                                        int command_id,
                                        const std::u16string& label) {
  InsertItemAtIndex(Item(command_id, TYPE_CHECK, label), index);
}

void SimpleMenuModel::InsertCheckItemWithStringIdAt(size_t index,
                                                    int command_id,
                                                    int string_id) {
  InsertCheckItemAt(index, command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::InsertRadioItemAt(size_t index,
                                        int command_id,
                                        const std::u16string& label,
                                        int group_id) {
  Item item(command_id, TYPE_RADIO, label);
  item.group_id = group_id;
  InsertItemAtIndex(std::move(item), index);
}

void SimpleMenuModel::InsertRadioItemWithStringIdAt(size_t index,
                                                    int command_id,
                                                    int string_id,
                                                    int group_id) {
  InsertRadioItemAt(
      index, command_id, l10n_util::GetStringUTF16(string_id), group_id);
}

void SimpleMenuModel::InsertTitleWithStringIdAt(size_t index, int string_id) {
  Item item(kTitleId, TYPE_TITLE, l10n_util::GetStringUTF16(string_id));
  // Titles are non-interactive and should not be enabled.
  item.enabled = false;
  InsertItemAtIndex(std::move(item), index);
}

void SimpleMenuModel::InsertSeparatorAt(size_t index,
                                        MenuSeparatorType separator_type) {
#if !defined(USE_AURA)
  if (separator_type != NORMAL_SEPARATOR) {
    NOTIMPLEMENTED();
  }
#endif
  Item item(kSeparatorId, TYPE_SEPARATOR, std::u16string());
  item.separator_type = separator_type;
  InsertItemAtIndex(std::move(item), index);
}

void SimpleMenuModel::InsertSubMenuAt(size_t index,
                                      int command_id,
                                      const std::u16string& label,
                                      MenuModel* model) {
  Item item(command_id, TYPE_SUBMENU, label);
  item.submenu = model;
  InsertItemAtIndex(std::move(item), index);
}

void SimpleMenuModel::InsertSubMenuWithStringIdAt(size_t index,
                                                  int command_id,
                                                  int string_id,
                                                  MenuModel* model) {
  InsertSubMenuAt(index, command_id, l10n_util::GetStringUTF16(string_id),
                  model);
}

void SimpleMenuModel::RemoveItemAt(size_t index) {
  items_.erase(items_.begin() +
               static_cast<ptrdiff_t>(ValidateItemIndex(index)));
  MenuItemsChanged();
}

void SimpleMenuModel::SetIcon(size_t index, const ui::ImageModel& icon) {
  items_[ValidateItemIndex(index)].icon = icon;
  MenuItemsChanged();
}

void SimpleMenuModel::SetLabel(size_t index, const std::u16string& label) {
  items_[ValidateItemIndex(index)].label = label;
  MenuItemsChanged();
}

void SimpleMenuModel::SetMinorText(size_t index,
                                   const std::u16string& minor_text) {
  items_[ValidateItemIndex(index)].minor_text = minor_text;
}

void SimpleMenuModel::SetMinorIcon(size_t index,
                                   const ui::ImageModel& minor_icon) {
  items_[ValidateItemIndex(index)].minor_icon = minor_icon;
}

void SimpleMenuModel::SetEnabledAt(size_t index, bool enabled) {
  if (items_[ValidateItemIndex(index)].enabled == enabled)
    return;

  items_[index].enabled = enabled;
  MenuItemsChanged();
}

void SimpleMenuModel::SetVisibleAt(size_t index, bool visible) {
  if (items_[ValidateItemIndex(index)].visible == visible)
    return;

  items_[index].visible = visible;
  MenuItemsChanged();
}

void SimpleMenuModel::SetIsNewFeatureAt(size_t index,
                                        IsNewFeatureAtValue is_new_feature) {
  items_[ValidateItemIndex(index)].is_new_feature = is_new_feature;
}

void SimpleMenuModel::SetMayHaveMnemonicsAt(size_t index,
                                            bool may_have_mnemonics) {
  items_[ValidateItemIndex(index)].may_have_mnemonics = may_have_mnemonics;
}

void SimpleMenuModel::SetAccessibleNameAt(size_t index,
                                          std::u16string accessible_name) {
  items_[ValidateItemIndex(index)].accessible_name = std::move(accessible_name);
}

void SimpleMenuModel::SetElementIdentifierAt(size_t index,
                                             ElementIdentifier unique_id) {
  items_[ValidateItemIndex(index)].unique_id = unique_id;
}

void SimpleMenuModel::SetExecuteCallbackAt(
    size_t index,
    base::RepeatingCallback<void(int)> callback) {
  items_[ValidateItemIndex(index)].on_execute_callback = callback;
}

void SimpleMenuModel::Clear() {
  items_.clear();
  MenuItemsChanged();
}

std::optional<size_t> SimpleMenuModel::GetIndexOfCommandId(
    int command_id) const {
  for (auto i = items_.begin(); i != items_.end(); ++i) {
    if (i->command_id == command_id)
      return static_cast<size_t>(std::distance(items_.begin(), i));
  }
  return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, MenuModel implementation:

base::WeakPtr<ui::MenuModel> SimpleMenuModel::AsWeakPtr() {
  return method_factory_.GetWeakPtr();
}

size_t SimpleMenuModel::GetItemCount() const {
  return items_.size();
}

MenuModel::ItemType SimpleMenuModel::GetTypeAt(size_t index) const {
  return items_[ValidateItemIndex(index)].type;
}

ui::MenuSeparatorType SimpleMenuModel::GetSeparatorTypeAt(size_t index) const {
  return items_[ValidateItemIndex(index)].separator_type;
}

int SimpleMenuModel::GetCommandIdAt(size_t index) const {
  return items_[ValidateItemIndex(index)].command_id;
}

std::u16string SimpleMenuModel::GetLabelAt(size_t index) const {
  if (IsItemDynamicAt(index))
    return delegate_->GetLabelForCommandId(GetCommandIdAt(index));
  return items_[ValidateItemIndex(index)].label;
}

std::u16string SimpleMenuModel::GetMinorTextAt(size_t index) const {
  return items_[ValidateItemIndex(index)].minor_text;
}

ImageModel SimpleMenuModel::GetMinorIconAt(size_t index) const {
  return items_[ValidateItemIndex(index)].minor_icon;
}

bool SimpleMenuModel::IsItemDynamicAt(size_t index) const {
  return delegate_ &&
         delegate_->IsItemForCommandIdDynamic(GetCommandIdAt(index));
}

bool SimpleMenuModel::GetAcceleratorAt(size_t index,
                                       ui::Accelerator* accelerator) const {
  return delegate_ && delegate_->GetAcceleratorForCommandId(
                          GetCommandIdAt(index), accelerator);
}

bool SimpleMenuModel::IsItemCheckedAt(size_t index) const {
  if (!delegate_)
    return false;
  MenuModel::ItemType item_type = GetTypeAt(index);
  return (item_type == TYPE_CHECK || item_type == TYPE_RADIO) &&
         delegate_->IsCommandIdChecked(GetCommandIdAt(index));
}

int SimpleMenuModel::GetGroupIdAt(size_t index) const {
  return items_[ValidateItemIndex(index)].group_id;
}

ImageModel SimpleMenuModel::GetIconAt(size_t index) const {
  if (IsItemDynamicAt(index))
    return delegate_->GetIconForCommandId(GetCommandIdAt(index));
  return items_[ValidateItemIndex(index)].icon;
}

ButtonMenuItemModel* SimpleMenuModel::GetButtonMenuItemAt(size_t index) const {
  return items_[ValidateItemIndex(index)].button_model;
}

bool SimpleMenuModel::IsEnabledAt(size_t index) const {
  int command_id = GetCommandIdAt(index);

  if (!delegate_ || command_id == kSeparatorId || command_id == kTitleId ||
      GetButtonMenuItemAt(index)) {
    return items_[ValidateItemIndex(index)].enabled;
  }

  return delegate_->IsCommandIdEnabled(command_id) &&
         items_[ValidateItemIndex(index)].enabled;
}

bool SimpleMenuModel::IsVisibleAt(size_t index) const {
  int command_id = GetCommandIdAt(index);
  if (!delegate_ || command_id == kSeparatorId || command_id == kTitleId ||
      GetButtonMenuItemAt(index)) {
    return items_[ValidateItemIndex(index)].visible;
  }

  return delegate_->IsCommandIdVisible(command_id) &&
         items_[ValidateItemIndex(index)].visible;
}

bool SimpleMenuModel::IsAlertedAt(size_t index) const {
  const int command_id = GetCommandIdAt(index);
  if (!delegate_ || command_id == kSeparatorId || command_id == kTitleId)
    return false;

  // This method needs to be recursive, because if the highlighted command is
  // in a submenu then the submenu item should also be highlighted.
  if (auto* const submenu = GetSubmenuModelAt(index)) {
    for (size_t i = 0; i < submenu->GetItemCount(); ++i) {
      if (submenu->IsAlertedAt(i))
        return true;
    }
  }

  // A submenu may assign element identifiers to menu items. This
  // information needs to be shared with the delegate which may want to
  // highlight specific elements keyed by identifier.
  const ui::ElementIdentifier element_id = GetElementIdentifierAt(index);
  if (element_id && delegate_->IsElementIdAlerted(element_id)) {
    return true;
  }

  return delegate_->IsCommandIdAlerted(command_id);
}

bool SimpleMenuModel::IsNewFeatureAt(size_t index) const {
  return items_[ValidateItemIndex(index)].is_new_feature;
}

bool SimpleMenuModel::MayHaveMnemonicsAt(size_t index) const {
  return items_[ValidateItemIndex(index)].may_have_mnemonics;
}

std::u16string SimpleMenuModel::GetAccessibleNameAt(size_t index) const {
  return items_[ValidateItemIndex(index)].accessible_name;
}

ElementIdentifier SimpleMenuModel::GetElementIdentifierAt(size_t index) const {
  return items_[ValidateItemIndex(index)].unique_id;
}

void SimpleMenuModel::ActivatedAt(size_t index) {
  ActivatedAt(index, 0);
}

void SimpleMenuModel::ActivatedAt(size_t index, int event_flags) {
  if (!delegate_) {
    return;
  }
  // The delegate might be destroyed after executing the command. Hence the
  // callback is temporarily copied. The delegate destruction is tested in
  // MenuControllerTest.OwningDelegate.
  const base::RepeatingCallback<void(int)> on_execute_callback =
      items_[ValidateItemIndex(index)].on_execute_callback;
  delegate_->ExecuteCommand(GetCommandIdAt(index), event_flags);
  if (on_execute_callback) {
    on_execute_callback.Run(event_flags);
  }
}

MenuModel* SimpleMenuModel::GetSubmenuModelAt(size_t index) const {
  return items_[ValidateItemIndex(index)].submenu;
}

void SimpleMenuModel::MenuWillShow() {
  if (delegate_)
    delegate_->OnMenuWillShow(this);
}

void SimpleMenuModel::MenuWillClose() {
  // Due to how menus work on the different platforms, ActivatedAt will be
  // called after this.  It's more convenient for the delegate to be called
  // afterwards though, so post a task.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SimpleMenuModel::OnMenuClosed,
                                method_factory_.GetWeakPtr()));
}

void SimpleMenuModel::OnMenuClosed() {
  if (delegate_)
    delegate_->MenuClosed(this);
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, Protected:

void SimpleMenuModel::MenuItemsChanged() {
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, Private:

SimpleMenuModel::Item::Item(Item&&) = default;
SimpleMenuModel::Item::Item(int command_id, ItemType type, std::u16string label)
    : command_id(command_id), type(type), label(label) {}
SimpleMenuModel::Item& SimpleMenuModel::Item::operator=(Item&&) = default;
SimpleMenuModel::Item::~Item() = default;

size_t SimpleMenuModel::ValidateItemIndex(size_t index) const {
  CHECK_LT(index, items_.size());
  return index;
}

void SimpleMenuModel::AppendItem(Item item) {
  ValidateItem(item);
  items_.push_back(std::move(item));
  MenuItemsChanged();
}

void SimpleMenuModel::InsertItemAtIndex(Item item, size_t index) {
  ValidateItem(item);
  items_.insert(items_.begin() + static_cast<ptrdiff_t>(index),
                std::move(item));
  MenuItemsChanged();
}

void SimpleMenuModel::ValidateItem(const Item& item) {
#if DCHECK_IS_ON()
  if (item.type == TYPE_SEPARATOR) {
    DCHECK_EQ(item.command_id, kSeparatorId);
  } else if (item.type == TYPE_TITLE) {
    DCHECK_EQ(item.command_id, kTitleId);
  } else {
    DCHECK_GE(item.command_id, 0);
  }
#endif  // DCHECK_IS_ON()
}

}  // namespace ui
