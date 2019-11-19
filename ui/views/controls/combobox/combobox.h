// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/style/typography.h"

namespace gfx {
class FontList;
}

namespace ui {
class ComboboxModel;
class MenuModel;
}

namespace views {
namespace test {
class ComboboxTestApi;
}

class ComboboxListener;
class FocusRing;
class MenuRunner;
class PrefixSelector;

// A non-editable combobox (aka a drop-down list or selector).
// Combobox has two distinct parts, the drop down arrow and the text.
class VIEWS_EXPORT Combobox : public View,
                              public PrefixDelegate,
                              public ButtonListener,
                              public ui::ComboboxModelObserver {
 public:
  METADATA_HEADER(Combobox);

  static constexpr int kDefaultComboboxTextContext = style::CONTEXT_BUTTON;
  static constexpr int kDefaultComboboxTextStyle = style::STYLE_PRIMARY;

  // |model| is owned by the combobox when using this constructor.
  explicit Combobox(std::unique_ptr<ui::ComboboxModel> model,
                    int text_context = kDefaultComboboxTextContext,
                    int text_style = kDefaultComboboxTextStyle);
  // |model| is not owned by the combobox when using this constructor.
  explicit Combobox(ui::ComboboxModel* model,
                    int text_context = kDefaultComboboxTextContext,
                    int text_style = kDefaultComboboxTextStyle);
  ~Combobox() override;

  const gfx::FontList& GetFontList() const;

  // Sets the listener which will be called when a selection has been made.
  void set_listener(ComboboxListener* listener) { listener_ = listener; }

  // Gets/Sets the selected index.
  int GetSelectedIndex() const { return selected_index_; }
  void SetSelectedIndex(int index);

  // Looks for the first occurrence of |value| in |model()|. If found, selects
  // the found index and returns true. Otherwise simply noops and returns false.
  bool SelectValue(const base::string16& value);

  ui::ComboboxModel* model() const { return model_; }

  // Set the tooltip text, and the accessible name if it is currently empty.
  void SetTooltipText(const base::string16& tooltip_text);

  // Set the accessible name of the combobox.
  void SetAccessibleName(const base::string16& name);
  base::string16 GetAccessibleName() const;

  // Visually marks the combobox as having an invalid value selected.
  // When invalid, it paints with white text on a red background.
  // Callers are responsible for restoring validity with selection changes.
  void SetInvalid(bool invalid);
  bool GetInvalid() const { return invalid_; }

  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) override;
  bool OnKeyPressed(const ui::KeyEvent& e) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnThemeChanged() override;

  // Overridden from PrefixDelegate:
  int GetRowCount() override;
  int GetSelectedRow() override;
  void SetSelectedRow(int row) override;
  base::string16 GetTextForRow(int row) override;

  // Overridden from ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 protected:
  void set_size_to_largest_label(bool size_to_largest_label) {
    size_to_largest_label_ = size_to_largest_label;
  }

  // Overridden from ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override;

 private:
  friend class test::ComboboxTestApi;

  class ComboboxMenuModel;

  // Updates the border according to the current node_data.
  void UpdateBorder();

  // Given bounds within our View, this helper mirrors the bounds if necessary.
  void AdjustBoundsForRTLUI(gfx::Rect* rect) const;

  // Draws the selected value of the drop down list
  void PaintText(gfx::Canvas* canvas);

  // Show the drop down list
  void ShowDropDownMenu(ui::MenuSourceType source_type);

  // Cleans up after the menu as closed
  void OnMenuClosed(Button::ButtonState original_button_state);

  // Called when the selection is changed by the user.
  void OnPerformAction();

  // Finds the size of the largest menu label.
  gfx::Size GetContentSize() const;

  // Handles the clicking event.
  void HandleClickEvent();

  PrefixSelector* GetPrefixSelector();

  // Returns the color to use for the combobox's focus ring.
  SkColor GetFocusRingColor() const;

  // Optionally used to tie the lifetime of the model to this combobox. See
  // constructor.
  std::unique_ptr<ui::ComboboxModel> owned_model_;

  // Reference to our model, which may be owned or not.
  ui::ComboboxModel* model_;

  // Typography context for the text written in the combobox and the options
  // shown in the drop-down menu.
  const int text_context_;

  // Typography style for the text written in the combobox and the options shown
  // in the drop-down menu.
  const int text_style_;

  // Our listener. Not owned. Notified when the selected index change.
  ComboboxListener* listener_;

  // The current selected index; -1 and means no selection.
  int selected_index_;

  // True when the selection is visually denoted as invalid.
  bool invalid_;

  // The accessible name of this combobox.
  base::string16 accessible_name_;

  // A helper used to select entries by keyboard input.
  std::unique_ptr<PrefixSelector> selector_;

  // The ComboboxModel for use by |menu_runner_|.
  std::unique_ptr<ui::MenuModel> menu_model_;

  // Like MenuButton, we use a time object in order to keep track of when the
  // combobox was closed. The time is used for simulating menu behavior; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in Windows,
  // the button is not part of the displayed menu.
  base::TimeTicks closed_time_;

  // The maximum dimensions of the content in the dropdown.
  gfx::Size content_size_;

  // A transparent button that handles events and holds button state. Placed on
  // top of the combobox as a child view. Doesn't paint itself, but serves as a
  // host for inkdrops.
  Button* arrow_button_;

  // Set while the dropdown is showing. Ensures the menu is closed if |this| is
  // destroyed.
  std::unique_ptr<MenuRunner> menu_runner_;

  // When true, the size of contents is defined by the selected label.
  // Otherwise, it's defined by the widest label in the menu. If this is set to
  // true, the parent view must relayout in ChildPreferredSizeChanged().
  bool size_to_largest_label_;

  // The focus ring for this Combobox.
  std::unique_ptr<FocusRing> focus_ring_;

  DISALLOW_COPY_AND_ASSIGN(Combobox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
