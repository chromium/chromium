// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_SIMPLE_MENU_MODEL_H_
#define UI_BASE_MODELS_SIMPLE_MENU_MODEL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"

namespace user_education {
class NewBadgeController;
}

class ChromeLabsViewController;

namespace ui {

class ButtonMenuItemModel;

// Boolean-like value that determines if a menu item should be marked as
// "new". Cannot be constructed in a truthy sate except by a fixed set of
// classes, forcing the use of those classes (or wrappers around those classes)
// in order to display as new. This prevents creating menu items which are
// marked as "new" for an extended/indefinite period of time.
class COMPONENT_EXPORT(UI_BASE) IsNewFeatureAtValue {
 public:
  IsNewFeatureAtValue() = default;

  // Allowed factory classes:
  IsNewFeatureAtValue(base::PassKey<user_education::NewBadgeController>,
                      bool value)
      : ui::IsNewFeatureAtValue(value) {}
  IsNewFeatureAtValue(base::PassKey<ChromeLabsViewController>, bool value)
      : ui::IsNewFeatureAtValue(value) {}

  // For other platforms/factories, add appropriate PassKey.

  IsNewFeatureAtValue(const IsNewFeatureAtValue&) = default;
  IsNewFeatureAtValue& operator=(const IsNewFeatureAtValue&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  /* implicit */ operator bool() const { return value_; }
  bool operator!() const { return !value_; }

  static IsNewFeatureAtValue create_for_test(bool value) {
    IsNewFeatureAtValue result;
    result.value_ = value;
    return result;
  }

 private:
  explicit IsNewFeatureAtValue(bool value) : value_(value) {}

  bool value_ = false;
};

// A simple MenuModel implementation with an imperative API for adding menu
// items. This makes it easy to construct fixed menus. Menus populated by
// dynamic data sources may be better off implementing MenuModel directly.
// The breadth of MenuModel is not exposed through this API.
class COMPONENT_EXPORT(UI_BASE) SimpleMenuModel : public MenuModel {
 public:
  // Default icon size to be used for context menus.
  static constexpr int kDefaultIconSize = 16;

  class COMPONENT_EXPORT(UI_BASE) Delegate : public AcceleratorProvider {
   public:
    ~Delegate() override = default;

    // Makes |command_id| appear toggled true if it's a "check" or "radio" type
    // menu item. This has no effect for menu items with no boolean state.
    virtual bool IsCommandIdChecked(int command_id) const;

    // Delegate should return true if |command_id| should be enabled.
    virtual bool IsCommandIdEnabled(int command_id) const;

    // Delegate should return true if |command_id| should be visible.
    virtual bool IsCommandIdVisible(int command_id) const;

    // Determines if |command_id| should be rendered with an alert for
    // in-product help.
    virtual bool IsCommandIdAlerted(int command_id) const;

    // Determines if |element_id| should be rendered with an alert for
    // in-product help.
    virtual bool IsElementIdAlerted(ui::ElementIdentifier element_id) const;

    // Some command ids have labels and icons that change over time.
    virtual bool IsItemForCommandIdDynamic(int command_id) const;
    virtual std::u16string GetLabelForCommandId(int command_id) const;
    // Gets the icon for the item with the specified id.
    virtual ImageModel GetIconForCommandId(int command_id) const;

    // Performs the action associates with the specified command id.
    // The passed |event_flags| are the flags from the event which issued this
    // command and they can be examined to find modifier keys.
    virtual void ExecuteCommand(int command_id, int event_flags) = 0;

    // Notifies the delegate that the menu is about to show.
    // Slight hack: Prefix with "On" to make sure this doesn't conflict with
    // MenuModel::MenuWillShow(), since many classes derive from both
    // SimpleMenuModel and SimpleMenuModel::Delegate.
    virtual void OnMenuWillShow(SimpleMenuModel* source);

    // Notifies the delegate that the menu has closed.
    virtual void MenuClosed(SimpleMenuModel* source);

    // AcceleratorProvider overrides:
    // By default, returns false for all commands. Can be further overridden.
    bool GetAcceleratorForCommandId(
        int command_id,
        ui::Accelerator* accelerator) const override;
  };

  // The Delegate can be NULL, though if it is items can't be checked or
  // disabled.
  explicit SimpleMenuModel(Delegate* delegate);

  SimpleMenuModel(const SimpleMenuModel&) = delete;
  SimpleMenuModel& operator=(const SimpleMenuModel&) = delete;

  ~SimpleMenuModel() override;

  // Methods for adding items to the model.
  void AddItem(int command_id, const std::u16string& label);
  void AddItemWithStringId(int command_id, int string_id);
  void AddItemWithIcon(int command_id,
                       const std::u16string& label,
                       const ui::ImageModel& icon);
  void AddItemWithStringIdAndIcon(int command_id,
                                  int string_id,
                                  const ui::ImageModel& icon);
  void AddCheckItem(int command_id, const std::u16string& label);
  void AddCheckItemWithStringId(int command_id, int string_id);
  void AddRadioItem(int command_id, const std::u16string& label, int group_id);
  void AddRadioItemWithStringId(int command_id, int string_id, int group_id);
  void AddHighlightedItemWithIcon(int command_id,
                                  const std::u16string& label,
                                  const ui::ImageModel& icon);
  void AddTitle(const std::u16string& label);
  void AddTitleWithStringId(int string_id);

  // Adds a separator of the specified type to the model.
  // - Adding a separator after another separator is always invalid if they
  //   differ in type, but silently ignored if they are both NORMAL.
  // - Adding a separator to an empty model is invalid, unless they are NORMAL
  //   or SPACING. NORMAL separators are silently ignored if the model is empty.
  void AddSeparator(MenuSeparatorType separator_type);

  // These methods take pointers to various sub-models. These models should be
  // owned by the same owner of this SimpleMenuModel.
  void AddButtonItem(int command_id, ButtonMenuItemModel* model);
  void AddSubMenu(int command_id,
                  const std::u16string& label,
                  MenuModel* model);
  void AddSubMenuWithStringId(int command_id, int string_id, MenuModel* model);
  void AddSubMenuWithIcon(int command_id,
                          const std::u16string& label,
                          MenuModel* model,
                          const ImageModel& icon);
  void AddSubMenuWithStringIdAndIcon(int command_id,
                                     int string_id,
                                     MenuModel* model,
                                     const ui::ImageModel& icon);
  void AddActionableSubMenu(int command_id,
                            const std::u16string& label,
                            MenuModel* model);
  void AddActionableSubmenuWithStringIdAndIcon(int command_id,
                                               int string_id,
                                               MenuModel* model,
                                               const ui::ImageModel& icon);

  // Methods for inserting items into the model.
  void InsertItemAt(size_t index, int command_id, const std::u16string& label);
  void InsertItemWithStringIdAt(size_t index, int command_id, int string_id);
  void InsertCheckItemAt(size_t index,
                         int command_id,
                         const std::u16string& label);
  void InsertCheckItemWithStringIdAt(size_t index,
                                     int command_id,
                                     int string_id);
  void InsertRadioItemAt(size_t index,
                         int command_id,
                         const std::u16string& label,
                         int group_id);
  void InsertRadioItemWithStringIdAt(size_t index,
                                     int command_id,
                                     int string_id,
                                     int group_id);
  void InsertTitleWithStringIdAt(size_t index, int string_id);
  void InsertSeparatorAt(size_t index, MenuSeparatorType separator_type);
  void InsertSubMenuAt(size_t index,
                       int command_id,
                       const std::u16string& label,
                       MenuModel* model);
  void InsertSubMenuWithStringIdAt(size_t index,
                                   int command_id,
                                   int string_id,
                                   MenuModel* model);

  // Remove item at specified index from the model.
  void RemoveItemAt(size_t index);

  // Sets the icon for the item at |index|.
  void SetIcon(size_t index, const ui::ImageModel& icon);

  // Sets the label for the item at |index|.
  void SetLabel(size_t index, const std::u16string& label);

  // Sets the minor text for the item at |index|.
  void SetMinorText(size_t index, const std::u16string& minor_text);

  // Sets the minor icon for the item at |index|.
  void SetMinorIcon(size_t index, const ui::ImageModel& minor_icon);

  // Sets whether the item at |index| is enabled.
  void SetEnabledAt(size_t index, bool enabled);

  // Sets whether the item at |index| is visible.
  void SetVisibleAt(size_t index, bool visible);

  // Sets whether the item at |index| is new.
  void SetIsNewFeatureAt(size_t index, IsNewFeatureAtValue is_new_feature);

  // Sets whether the item at |index| is may have mnemonics.
  void SetMayHaveMnemonicsAt(size_t index, bool may_have_mnemonics);

  // Sets the accessible name of item at |index|.
  void SetAccessibleNameAt(size_t index, std::u16string accessible_name);

  // Sets an application-window unique identifier associated with this menu item
  // allowing it to be tracked without knowledge of menu-specific command IDs.
  void SetElementIdentifierAt(size_t index, ElementIdentifier unique_id);

  // Sets the callback that will be run after the menu item has been executed.
  void SetExecuteCallbackAt(size_t index,
                            base::RepeatingCallback<void(int)> callback);

  // Clears all items. Note that it does not free MenuModel of submenu.
  void Clear();

  // Returns the index of the item that has the given |command_id|. Returns
  // nullopt if not found.
  std::optional<size_t> GetIndexOfCommandId(int command_id) const;

  // Overridden from MenuModel:
  base::WeakPtr<ui::MenuModel> AsWeakPtr() override;
  size_t GetItemCount() const override;
  ItemType GetTypeAt(size_t index) const override;
  ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const override;
  int GetCommandIdAt(size_t index) const override;
  std::u16string GetLabelAt(size_t index) const override;
  std::u16string GetMinorTextAt(size_t index) const override;
  ImageModel GetMinorIconAt(size_t index) const override;
  bool IsItemDynamicAt(size_t index) const override;
  bool GetAcceleratorAt(size_t index,
                        ui::Accelerator* accelerator) const override;
  bool IsItemCheckedAt(size_t index) const override;
  int GetGroupIdAt(size_t index) const override;
  ImageModel GetIconAt(size_t index) const override;
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const override;
  bool IsEnabledAt(size_t index) const override;
  bool IsVisibleAt(size_t index) const override;
  bool IsAlertedAt(size_t index) const override;
  bool IsNewFeatureAt(size_t index) const override;
  bool MayHaveMnemonicsAt(size_t index) const override;
  std::u16string GetAccessibleNameAt(size_t index) const override;
  ElementIdentifier GetElementIdentifierAt(size_t index) const override;
  void ActivatedAt(size_t index) override;
  void ActivatedAt(size_t index, int event_flags) override;
  MenuModel* GetSubmenuModelAt(size_t index) const override;
  void MenuWillShow() override;
  void MenuWillClose() override;

 protected:
  Delegate* delegate() { return delegate_; }

  // One or more of the menu menu items associated with the model has changed.
  // Do any handling if necessary.
  virtual void MenuItemsChanged();

 private:
  struct Item {
    Item(Item&&);
    Item(int command_id, ItemType type, std::u16string label);
    Item& operator=(Item&&);
    ~Item();

    int command_id = 0;
    ItemType type = TYPE_COMMAND;
    std::u16string label;
    std::u16string minor_text;
    ImageModel minor_icon;
    ImageModel icon;
    int group_id = -1;
    raw_ptr<MenuModel, DanglingUntriaged> submenu = nullptr;
    raw_ptr<ButtonMenuItemModel, DanglingUntriaged> button_model = nullptr;
    MenuSeparatorType separator_type = NORMAL_SEPARATOR;
    bool enabled = true;
    bool visible = true;
    bool is_new_feature = false;
    bool may_have_mnemonics = true;
    std::u16string accessible_name;
    ElementIdentifier unique_id;
    base::RepeatingCallback<void(int)> on_execute_callback;
  };

  using ItemVector = std::vector<Item>;

  // Returns |index|.
  size_t ValidateItemIndex(size_t index) const;

  // Functions for inserting items into |items_|.
  void AppendItem(Item item);
  void InsertItemAtIndex(Item item, size_t index);
  void ValidateItem(const Item& item);

  // Notify the delegate that the menu is closed.
  void OnMenuClosed();

  ItemVector items_;

  raw_ptr<Delegate, AcrossTasksDanglingUntriaged> delegate_;

  base::WeakPtrFactory<SimpleMenuModel> method_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_MODELS_SIMPLE_MENU_MODEL_H_
