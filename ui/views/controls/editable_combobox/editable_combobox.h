// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/metadata/view_factory.h"
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
class MenuModel;
}  // namespace ui

namespace views {
class Button;
class MenuRunner;
class Textfield;

namespace test {
class InteractionTestUtilSimulatorViews;
}  // namespace test

// Textfield that also shows a drop-down list with suggestions.
class VIEWS_EXPORT EditableCombobox : public View,
                                      public TextfieldController,
                                      public ViewObserver,
                                      public AnimatingLayoutManager::Observer,
                                      public LayoutDelegate {
  METADATA_HEADER(EditableCombobox, View)

 public:
  // A strategy that can be used to customize the display of the drop-down menu.
  // It is only intended to be used by classes that extend `EditableCombobox`.
  class MenuDecorationStrategy {
   public:
    virtual ~MenuDecorationStrategy() = default;

    virtual std::u16string DecorateItemText(std::u16string text) const;
  };

  static constexpr int kDefaultTextContext = style::CONTEXT_BUTTON;
  static constexpr int kDefaultTextStyle = style::STYLE_PRIMARY;

  EditableCombobox();

  // |combobox_model|: The ComboboxModel that gives us the items to show in the
  // drop-down list.
  // |filter_on_edit|: Whether to only show the items that are case-insensitive
  // completions of the current textfield content.
  // |show_on_empty|: Whether to show the drop-down list when there is no
  // textfield content.
  // |text_context| and |text_style|: Together these indicate the font to use.
  // |display_arrow|: Whether to display an arrow in the combobox to indicate
  // that there is a drop-down list.
  explicit EditableCombobox(std::unique_ptr<ui::ComboboxModel> combobox_model,
                            bool filter_on_edit = false,
                            bool show_on_empty = true,
                            int text_context = kDefaultTextContext,
                            int text_style = kDefaultTextStyle,
                            bool display_arrow = true);

  EditableCombobox(const EditableCombobox&) = delete;
  EditableCombobox& operator=(const EditableCombobox&) = delete;

  ~EditableCombobox() override;

  void SetModel(std::unique_ptr<ui::ComboboxModel> model);

  const std::u16string& GetText() const;
  void SetText(const std::u16string& text);

  void SetInvalid(bool invalid);

  const std::u16string& GetPlaceholderText() const;
  void SetPlaceholderText(const std::u16string& text);

  const gfx::FontList& GetFontList() const;

  void SetCallback(base::RepeatingClosure callback) {
    content_changed_callback_ = std::move(callback);
  }

  // Selects the specified logical text range for the textfield.
  void SelectRange(const gfx::Range& range);

 protected:
  // Sets the menu decoration strategy. Setting it triggers an update to the
  // menu.
  void SetMenuDecorationStrategy(
      std::unique_ptr<MenuDecorationStrategy> strategy);

  // Forces an update of the drop-down menu.
  void UpdateMenu();

  // Adds `view` to the set of controls. The ordering is such that views are
  // added to the front (i.e. to the left in LTR set-ups).
  template <typename T>
  T* AddControlElement(std::unique_ptr<T> view) {
    T* raw_view =
        control_elements_container_->AddChildViewAt(std::move(view), 0);
    UpdateTextfieldInsets();
    return raw_view;
  }

  Textfield& GetTextfield() { return *textfield_; }

  Button* GetArrowButtonForTesting() { return arrow_; }

  // View:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;

 private:
  friend class EditableComboboxTest;
  friend class EditablePasswordComboboxTest;
  friend class test::InteractionTestUtilSimulatorViews;
  FRIEND_TEST_ALL_PREFIXES(EditableComboboxTest, AccessibleNameAndRole);

  class EditableComboboxMenuModel;
  class EditableComboboxPreTargetHandler;

  void CloseMenu();

  // Called when an item is selected from the menu.
  void OnItemSelected(size_t index);

  // Notifies listener of new content and updates the menu items to show.
  void HandleNewContent(const std::u16string& new_content);

  // Toggles the dropdown menu in response to |event|.
  void ArrowButtonPressed(const ui::Event& event);

  // Shows the drop-down menu.
  void ShowDropDownMenu(ui::MenuSourceType source_type = ui::MENU_SOURCE_NONE);

  // Recalculates the extra insets of the textfield based on the size of the
  // controls container.
  void UpdateTextfieldInsets();

  // These are for unit tests to get data from private implementation classes.
  const ui::MenuModel* GetMenuModelForTesting() const;
  std::u16string GetItemTextForTesting(size_t index) const;

  // Returns the underlying combobox model. Used only by
  // `ui::test::InteractionTestUtil`.
  const ui::ComboboxModel* GetComboboxModel() const;

  void RequestFocus() override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnAccessibleNameChanged(const std::u16string& new_name) override;

  // Overridden from TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Overridden from ViewObserver:
  void OnViewBlurred(View* observed_view) override;

  // Overridden from views::AnimatingLayoutManager::Observer:
  void OnLayoutIsAnimatingChanged(views::AnimatingLayoutManager* source,
                                  bool is_animating) override;

  // Overridden from LayoutDelegate:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

  bool ShouldApplyInkDropEffects();

  raw_ptr<Textfield> textfield_;
  raw_ptr<BoxLayoutView> control_elements_container_ = nullptr;
  raw_ptr<Button> arrow_ = nullptr;

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

  // Whether to adapt the items shown to the textfield content.
  const bool filter_on_edit_;

  // Whether to show options when the textfield is empty.
  const bool show_on_empty_;

  // Set while the drop-down is showing.
  std::unique_ptr<MenuRunner> menu_runner_;

  base::RepeatingClosure content_changed_callback_;

  bool dropdown_blocked_for_animation_ = false;

  base::ScopedObservation<View, ViewObserver> observation_{this};
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, EditableCombobox, View)
VIEW_BUILDER_PROPERTY(base::RepeatingClosure, Callback)
VIEW_BUILDER_PROPERTY(std::unique_ptr<ui::ComboboxModel>, Model)
VIEW_BUILDER_PROPERTY(std::u16string, PlaceholderText)
VIEW_BUILDER_PROPERTY(std::u16string, Text)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, EditableCombobox)

#endif  // UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_
