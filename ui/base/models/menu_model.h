// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_MENU_MODEL_H_
#define UI_BASE_MODELS_MENU_MODEL_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class FontList;
class Image;
struct VectorIcon;
}

namespace ui {

class Accelerator;
class ButtonMenuItemModel;

// An interface implemented by an object that provides the content of a menu.
class UI_BASE_EXPORT MenuModel : public base::SupportsWeakPtr<MenuModel> {
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

  MenuModel();

  virtual ~MenuModel();

  // Returns true if any of the items within the model have icons. Not all
  // platforms support icons in menus natively and so this is a hint for
  // triggering a custom rendering mode.
  virtual bool HasIcons() const = 0;

  // Returns the number of items in the menu.
  virtual int GetItemCount() const = 0;

  // Returns the type of item at the specified index.
  virtual ItemType GetTypeAt(int index) const = 0;

  // Returns the separator type at the specified index.
  virtual ui::MenuSeparatorType GetSeparatorTypeAt(int index) const = 0;

  // Returns the command id of the item at the specified index.
  virtual int GetCommandIdAt(int index) const = 0;

  // Returns the label of the item at the specified index.
  virtual base::string16 GetLabelAt(int index) const = 0;

  // Returns the minor text of the item at the specified index. The minor text
  // is rendered to the right of the label and using the font GetLabelFontAt().
  virtual base::string16 GetMinorTextAt(int index) const;

  // Returns the minor icon of the item at the specified index. The minor icon
  // is rendered to the left of the minor text.
  virtual const gfx::VectorIcon* GetMinorIconAt(int index) const;

  // Returns true if the menu item (label/sublabel/icon) at the specified
  // index can change over the course of the menu's lifetime. If this function
  // returns true, the label, sublabel and icon of the menu item will be
  // updated each time the menu is shown.
  virtual bool IsItemDynamicAt(int index) const = 0;

  // Returns the font list used for the label at the specified index.
  // If NULL, then the default font list should be used.
  virtual const gfx::FontList* GetLabelFontListAt(int index) const;

  // Gets the accelerator information for the specified index, returning true if
  // there is a shortcut accelerator for the item, false otherwise.
  virtual bool GetAcceleratorAt(int index,
                                ui::Accelerator* accelerator) const = 0;

  // Returns the checked state of the item at the specified index.
  virtual bool IsItemCheckedAt(int index) const = 0;

  // Returns the id of the group of radio items that the item at the specified
  // index belongs to.
  virtual int GetGroupIdAt(int index) const = 0;

  // Gets the icon for the item at the specified index, returning true if there
  // is an icon, false otherwise.
  virtual bool GetIconAt(int index, gfx::Image* icon) const = 0;

  // Gets the vector icon for the item at the specified index. At most one of
  // GetIconAt() and GetVectorIconAt() should be used for a single menu index.
  virtual const gfx::VectorIcon* GetVectorIconAt(int index) const;

  // Returns the model for a menu item with a line of buttons at |index|.
  virtual ButtonMenuItemModel* GetButtonMenuItemAt(int index) const = 0;

  // Returns the enabled state of the item at the specified index.
  virtual bool IsEnabledAt(int index) const = 0;

  // Returns true if the menu item is visible.
  virtual bool IsVisibleAt(int index) const;

  // Returns the model for the submenu at the specified index.
  virtual MenuModel* GetSubmenuModelAt(int index) const = 0;

  // Called when the item at the specified index has been activated.
  virtual void ActivatedAt(int index) = 0;

  // Called when the item has been activated with given event flags.
  // (for the case where the activation involves a navigation).
  // |event_flags| is a bit mask of ui::EventFlags.
  virtual void ActivatedAt(int index, int event_flags);

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
                                           int* index);

 private:
  // MenuModelDelegate. Weak. Could be null.
  MenuModelDelegate* menu_model_delegate_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_MENU_MODEL_H_
