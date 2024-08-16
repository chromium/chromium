// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_MENU_MODEL_H_
#define UI_BASE_MODELS_MENU_MODEL_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class FontList;
}

namespace ui {

class Accelerator;
class ButtonMenuItemModel;
class ImageModel;

// An interface implemented by an object that provides the content of a menu.
class COMPONENT_EXPORT(UI_BASE) MenuModel {
 public:
  // The type of item.
  enum ItemType {
    TYPE_COMMAND,      // Performs an action when selected.
    TYPE_CHECK,        // Can be selected/checked to toggle a boolean state.
    TYPE_RADIO,        // Can be selected/checked among a group of choices.
    TYPE_SEPARATOR,    // Shows a horizontal line separator.
    TYPE_BUTTON_ITEM,  // Shows a row of buttons.
    TYPE_SUBMENU,      // Presents a submenu within another menu.
    TYPE_ACTIONABLE_SUBMENU,  // A SUBMENU that is also a COMMAND.
    TYPE_HIGHLIGHTED,  // Performs an action when selected, and has a different
                       // colored background. When placed at the bottom, the
                       // background matches the menu's rounded corners.
    TYPE_TITLE,        // Plain text that does not perform any action when
                       // selected.
  };

  // ID to use for TYPE_TITLE items.
  static constexpr int kTitleId = -2;

  MenuModel();

  virtual ~MenuModel();

  // This must be implemented by the most concrete class.
  virtual base::WeakPtr<MenuModel> AsWeakPtr() = 0;

  // Returns the number of items in the menu.
  virtual size_t GetItemCount() const = 0;

  // Returns the type of item at the specified index.
  virtual ItemType GetTypeAt(size_t index) const = 0;

  // Returns the separator type at the specified index.
  virtual ui::MenuSeparatorType GetSeparatorTypeAt(size_t index) const = 0;

  // Returns the command id of the item at the specified index.
  virtual int GetCommandIdAt(size_t index) const = 0;

  // Returns the label of the item at the specified index.
  virtual std::u16string GetLabelAt(size_t index) const = 0;

  // Returns the secondary label of the item at the specified index. Secondary
  // label is shown below the label.
  virtual std::u16string GetSecondaryLabelAt(size_t index) const;

  // Returns the minor text of the item at the specified index. The minor text
  // is rendered to the right of the label and using the font GetLabelFontAt().
  virtual std::u16string GetMinorTextAt(size_t index) const;

  // Returns the minor icon of the item at the specified index. The minor icon
  // is rendered to the left of the minor text.
  virtual ImageModel GetMinorIconAt(size_t index) const;

  // Returns true if the menu item (label/sublabel/icon) at the specified
  // index can change over the course of the menu's lifetime. If this function
  // returns true, the label, sublabel and icon of the menu item will be
  // updated each time the menu is shown.
  virtual bool IsItemDynamicAt(size_t index) const = 0;

  // Returns whether the menu item at the specified index has a user-set title,
  // in which case it should always be treated as plain text.
  virtual bool MayHaveMnemonicsAt(size_t index) const;

  // Returns the accessible name for the menu item at the specified index.
  virtual std::u16string GetAccessibleNameAt(size_t index) const;

  // Returns the font list used for the label at the specified index.
  // If NULL, then the default font list should be used.
  virtual const gfx::FontList* GetLabelFontListAt(size_t index) const;

  // Gets the accelerator information for the specified index, returning true if
  // there is a shortcut accelerator for the item, false otherwise.
  virtual bool GetAcceleratorAt(size_t index,
                                ui::Accelerator* accelerator) const = 0;

  // Returns the checked state of the item at the specified index.
  virtual bool IsItemCheckedAt(size_t index) const = 0;

  // Returns the id of the group of radio items that the item at the specified
  // index belongs to.
  virtual int GetGroupIdAt(size_t index) const = 0;

  // Gets the icon for the item at the specified index. ImageModel is empty if
  // there is no icon.
  virtual ImageModel GetIconAt(size_t index) const = 0;

  // Returns the model for a menu item with a line of buttons at |index|.
  virtual ButtonMenuItemModel* GetButtonMenuItemAt(size_t index) const = 0;

  // Returns the enabled state of the item at the specified index.
  virtual bool IsEnabledAt(size_t index) const = 0;

  // Returns true if the menu item is visible.
  virtual bool IsVisibleAt(size_t index) const;

  // Returns true if the item is rendered specially to draw attention
  // for in-product help.
  virtual bool IsAlertedAt(size_t index) const;

  // Returns true if the menu item grants access to a new feature that we want
  // to show off to users (items marked as new will receive a "New" badge when
  // the appropriate flag is enabled).
  virtual bool IsNewFeatureAt(size_t index) const;

  // Returns an application-window-unique identifier that can be used to track
  // the menu item irrespective of menu-specific command IDs.
  virtual ElementIdentifier GetElementIdentifierAt(size_t index) const;

  // Returns the model for the submenu at the specified index.
  virtual MenuModel* GetSubmenuModelAt(size_t index) const = 0;

  // Called when the item at the specified index has been activated.
  virtual void ActivatedAt(size_t index) = 0;

  // Called when the item has been activated with given event flags.
  // (for the case where the activation involves a navigation).
  // |event_flags| is a bit mask of ui::EventFlags.
  virtual void ActivatedAt(size_t index, int event_flags);

  // Called when the menu is about to be shown.
  virtual void MenuWillShow() {}

  // Called when the menu is about to be closed. The MenuRunner, and |this|
  // should not be deleted here.
  virtual void MenuWillClose() {}

  // Set the MenuModelDelegate, owned by the caller of this function. We allow
  // setting a new one or clearing the current one.
  void SetMenuModelDelegate(MenuModelDelegate* delegate);

  // Gets the MenuModelDelegate.
  MenuModelDelegate* menu_model_delegate() { return menu_model_delegate_; }
  const MenuModelDelegate* menu_model_delegate() const {
    return menu_model_delegate_;
  }

  // Retrieves the model and index that contains a specific command id. Returns
  // true if an item with the specified command id is found. |model| is inout,
  // and specifies the model to start searching from.
  static bool GetModelAndIndexForCommandId(int command_id,
                                           MenuModel** model,
                                           size_t* index);

  virtual std::optional<ui::ColorId> GetForegroundColorId(size_t index);
  virtual std::optional<ui::ColorId> GetSubmenuBackgroundColorId(size_t index);
  virtual std::optional<ui::ColorId> GetSelectedBackgroundColorId(size_t index);

 private:
  // MenuModelDelegate. Weak. Could be null.
  raw_ptr<MenuModelDelegate> menu_model_delegate_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_MENU_MODEL_H_
