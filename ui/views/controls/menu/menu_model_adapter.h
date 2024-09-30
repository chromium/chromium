// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_MODEL_ADAPTER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_MODEL_ADAPTER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/menu_model_delegate.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/style/typography.h"

namespace ui {
class MenuModel;
}

namespace views {
class MenuItemView;

// This class wraps an instance of ui::MenuModel with the views::MenuDelegate
// interface required by views::MenuItemView.
class VIEWS_EXPORT MenuModelAdapter : public MenuDelegate,
                                      public ui::MenuModelDelegate {
 public:
  // The caller retains ownership of the ui::MenuModel instance and must ensure
  // it exists for the lifetime of the adapter. |this| will become the new
  // MenuModelDelegate of |menu_model| so that subsequent changes to it get
  // reflected in the created MenuItemView.
  explicit MenuModelAdapter(
      ui::MenuModel* menu_model,
      base::RepeatingClosure on_menu_closed_callback = base::NullCallback());

  MenuModelAdapter(const MenuModelAdapter&) = delete;
  MenuModelAdapter& operator=(const MenuModelAdapter&) = delete;

  ~MenuModelAdapter() override;

  // Populate a MenuItemView menu with the ui::MenuModel items
  // (including submenus).
  virtual void BuildMenu(MenuItemView* menu);

  // Creates, populates and returns a menu. Note that a raw pointer it kept
  // internally to be able to update the `MenuItemView` as response to calls to
  // `MenuModelDelegate::OnMenuStructureChanged()`.
  // returned MenuItemView.
  std::unique_ptr<MenuItemView> CreateMenu();

  void set_triggerable_event_flags(int triggerable_event_flags) {
    triggerable_event_flags_ = triggerable_event_flags;
  }
  int triggerable_event_flags() const { return triggerable_event_flags_; }

  // Creates a menu item for the specified entry in the model and adds it as
  // a child to |menu| at the specified |menu_index|.
  static MenuItemView* AddMenuItemFromModelAt(ui::MenuModel* model,
                                              size_t model_index,
                                              MenuItemView* menu,
                                              size_t menu_index,
                                              int item_id);

  // Creates a menu item for the specified entry in the model and appends it as
  // a child to |menu|.
  static MenuItemView* AppendMenuItemFromModel(ui::MenuModel* model,
                                               size_t model_index,
                                               MenuItemView* menu,
                                               int item_id);

  // MenuModelDelegate:
  void OnIconChanged(int command_id) override {}
  void OnMenuStructureChanged() override;
  void OnMenuClearingDelegate() override;

 protected:
  // Create and add a menu item to |menu| for the item at index |index| in
  // |model|. Subclasses override this to allow custom items to be added to the
  // menu.
  virtual MenuItemView* AppendMenuItem(MenuItemView* menu,
                                       ui::MenuModel* model,
                                       size_t index);

  // views::MenuDelegate implementation.
  void ExecuteCommand(int id) override;
  void ExecuteCommand(int id, int mouse_event_flags) override;
  bool IsTriggerableEvent(MenuItemView* source, const ui::Event& e) override;
  bool GetAccelerator(int id, ui::Accelerator* accelerator) const override;
  std::u16string GetLabel(int id) const override;
  const gfx::FontList* GetLabelFontList(int id) const override;
  bool IsCommandEnabled(int id) const override;
  bool IsCommandVisible(int id) const override;
  bool IsItemChecked(int id) const override;
  void WillShowMenu(MenuItemView* menu) override;
  void WillHideMenu(MenuItemView* menu) override;
  void OnMenuClosed(MenuItemView* menu) override;
  std::optional<SkColor> GetLabelColor(int command_id) const override;
  bool IsTearingDown() const override;

 private:
  // Implementation of BuildMenu().
  void BuildMenuImpl(MenuItemView* menu, ui::MenuModel* model);

  // Container of ui::MenuModel pointers as encountered by preorder
  // traversal.  The first element is always the top-level model
  // passed to the constructor.
  raw_ptr<ui::MenuModel, DanglingUntriaged> menu_model_;

  // Pointer to the `MenuItemView` created and updated by `this`, but not owned
  // by `this`.
  raw_ptr<MenuItemView, DanglingUntriaged> menu_ = nullptr;

  // Mouse event flags which can trigger menu actions.
  int triggerable_event_flags_;

  // Map MenuItems to MenuModels.  Used to implement WillShowMenu().
  std::map<MenuItemView*, raw_ptr<ui::MenuModel, CtnExperimental>> menu_map_;

  // Optional callback triggered during OnMenuClosed().
  base::RepeatingClosure on_menu_closed_callback_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_MODEL_ADAPTER_H_
