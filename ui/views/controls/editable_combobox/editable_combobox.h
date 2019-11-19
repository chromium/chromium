// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace gfx {
class FontList;
class Range;
}  // namespace gfx

namespace ui {
class ComboboxModel;
class Event;
}  // namespace ui

namespace views {
class EditableComboboxMenuModel;
class EditableComboboxListener;
class EditableComboboxPreTargetHandler;
class MenuRunner;
class Textfield;

// Textfield that also shows a drop-down list with suggestions.
class VIEWS_EXPORT EditableCombobox : public View,
                                      public TextfieldController,
                                      public ViewObserver,
                                      public ButtonListener {
 public:
  METADATA_HEADER(EditableCombobox);

  enum class Type {
    kRegular,
    kPassword,
  };

  static constexpr int kDefaultTextContext = style::CONTEXT_BUTTON;
  static constexpr int kDefaultTextStyle = style::STYLE_PRIMARY;

  // |combobox_model|: The ComboboxModel that gives us the items to show in the
  // menu.
  // |filter_on_edit|: Whether to only show the items that are case-insensitive
  // completions of the current textfield content.
  // |show_on_empty|: Whether to show the drop-down list when there is no
  // textfield content.
  // |type|: The EditableCombobox type.
  // |text_context| and |text_style|: Together these indicate the font to use.
  // |display_arrow|: Whether to display an arrow in the combobox to indicate
  // that there is a drop-down list.
  EditableCombobox(std::unique_ptr<ui::ComboboxModel> combobox_model,
                   bool filter_on_edit,
                   bool show_on_empty,
                   Type type = Type::kRegular,
                   int text_context = kDefaultTextContext,
                   int text_style = kDefaultTextStyle,
                   bool display_arrow = true);

  ~EditableCombobox() override;

  const base::string16& GetText() const;
  void SetText(const base::string16& text);

  const gfx::FontList& GetFontList() const;

  // Sets the listener that we will call when a selection is made.
  void set_listener(EditableComboboxListener* listener) {
    listener_ = listener;
  }

  // Selects the specified logical text range for the textfield.
  void SelectRange(const gfx::Range& range);

  // Sets the accessible name. Use SetAssociatedLabel instead if there is a
  // label associated with this combobox.
  void SetAccessibleName(const base::string16& name);

  // Sets the associated label; use this instead of SetAccessibleName if there
  // is a label associated with this combobox.
  void SetAssociatedLabel(View* labelling_view);

  // For Type::kPassword, sets whether the textfield and
  // drop-down menu will reveal their current content.
  void RevealPasswords(bool revealed);

  // Accessors of private members for tests.
  ui::ComboboxModel* GetComboboxModelForTest() { return combobox_model_.get(); }
  int GetItemCountForTest();
  base::string16 GetItemForTest(int index);
  MenuRunner* GetMenuRunnerForTest() { return menu_runner_.get(); }
  Textfield* GetTextfieldForTest() { return textfield_; }

 private:
  class EditableComboboxMenuModel;
  class EditableComboboxPreTargetHandler;

  void CloseMenu();

  // Called when an item is selected from the menu.
  void OnItemSelected(int index);

  // Notifies listener of new content and updates the menu items to show.
  void HandleNewContent(const base::string16& new_content);

  // Shows the drop-down menu.
  void ShowDropDownMenu(ui::MenuSourceType source_type = ui::MENU_SOURCE_NONE);

  // Overridden from View:
  void Layout() override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Overridden from ViewObserver:
  void OnViewBlurred(View* observed_view) override;

  // Overridden from ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  Textfield* textfield_;
  Button* arrow_ = nullptr;
  std::unique_ptr<ui::ComboboxModel> combobox_model_;

  // The EditableComboboxMenuModel used by |menu_runner_|.
  std::unique_ptr<EditableComboboxMenuModel> menu_model_;

  // Pre-target handler that closes the menu when press events happen in the
  // root view (outside of the open menu's boundaries) but not inside the
  // textfield.
  std::unique_ptr<EditableComboboxPreTargetHandler> pre_target_handler_;

  // Typography context for the text written in the textfield and the options
  // shown in the drop-down menu.
  const int text_context_;

  // Typography style for the text written in the textfield and the options
  // shown in the drop-down menu.
  const int text_style_;

  const Type type_;

  // Set while the drop-down is showing.
  std::unique_ptr<MenuRunner> menu_runner_;

  // Our listener. Not owned. Notified when the selected index changes.
  EditableComboboxListener* listener_ = nullptr;

  // Whether we are currently showing the passwords for type
  // Type::kPassword.
  bool showing_password_text_;

  DISALLOW_COPY_AND_ASSIGN(EditableCombobox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_
