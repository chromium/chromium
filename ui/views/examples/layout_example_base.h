// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_LAYOUT_EXAMPLE_BASE_H_
#define UI_VIEWS_EXAMPLES_LAYOUT_EXAMPLE_BASE_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view.h"

namespace views {
class Combobox;

namespace examples {

// Provides an example of a layout manager with arbitrary specific manager and
// controls. Lays out a sequence of ChildPanels in a view using the layout
// manager of choice.
class VIEWS_EXAMPLES_EXPORT LayoutExampleBase : public ExampleBase,
                                                public ButtonListener,
                                                public TextfieldController {
 public:
  // Grouping of multiple textfields that provide insets.
  struct InsetTextfields {
    Textfield* left = nullptr;
    Textfield* top = nullptr;
    Textfield* right = nullptr;
    Textfield* bottom = nullptr;
  };

  // This view is created and added to the left-side view in the FullPanel each
  // time the "Add" button is pressed. It also will display Textfield controls
  // when the mouse is pressed over the view. These Textfields allow the user to
  // interactively set each margin and the "flex" for the given view.
  class ChildPanel : public View, public TextfieldController {
   public:
    explicit ChildPanel(LayoutExampleBase* example);
    ChildPanel(const ChildPanel&) = delete;
    ChildPanel& operator=(const ChildPanel&) = delete;
    ~ChildPanel() override;

    // View:
    void Layout() override;
    bool OnMousePressed(const ui::MouseEvent& event) override;

    void SetSelected(bool value);
    bool selected() const { return selected_; }

    int GetFlex() const;

   protected:
    // View:
    void OnThemeChanged() override;

   private:
    // TextfieldController
    void ContentsChanged(Textfield* sender,
                         const base::string16& new_contents) override;

    Textfield* CreateTextfield();

    LayoutExampleBase* example_;
    bool selected_ = false;
    Textfield* flex_;
    InsetTextfields margin_;
    gfx::Size preferred_size_;
  };

  explicit LayoutExampleBase(const char* title);
  LayoutExampleBase(const LayoutExampleBase&) = delete;
  LayoutExampleBase& operator=(const LayoutExampleBase&) = delete;
  ~LayoutExampleBase() override;

  // Force the box_layout_panel_ to layout and repaint.
  void RefreshLayoutPanel(bool update_layout);

  static gfx::Insets TextfieldsToInsets(
      const InsetTextfields& textfields,
      const gfx::Insets& default_insets = gfx::Insets());

 protected:
  View* layout_panel() { return layout_panel_; }

  // Creates and adds a Combobox with a label with |label_text| to the left.
  // Sets |combobox_callback| as the callback for the created combobox.
  Combobox* CreateAndAddCombobox(const base::string16& label_text,
                                 const char* const* items,
                                 int count,
                                 base::RepeatingClosure combobox_callback);

  // Creates and adds a Textfield with a label with |label_text| to the left.
  Textfield* CreateAndAddTextfield(const base::string16& label_text);

  // Creates a set of labeled Textfields with |label_text|, and four text fields
  // arranged at compass points representing a set of insets. |textfields| is
  // populated with the fields that are created.
  void CreateMarginsTextFields(const base::string16& label_text,
                               InsetTextfields* textfields);

  // Creates and adds a Checkbox with label |label_text|.
  Checkbox* CreateAndAddCheckbox(const base::string16& label_text);

  // ButtonListener:
  // Be sure to call LayoutExampleBase::ButtonPressed() to ensure the "add"
  // button works correctly.
  void ButtonPressed(Button* sender, const ui::Event& event) final;

  // ExampleBase:
  // Be sure to call LayoutExampleBase::CreateExampleView() to ensure default
  // controls are created correctly.
  void CreateExampleView(View* container) final;

  gfx::Size GetNewChildPanelPreferredSize();

  // Called by CreateExampleView() to create any additional controls required by
  // the specific layout.
  virtual void CreateAdditionalControls() = 0;

  // Handles buttons added by derived classes after button handling for
  // common controls is done.
  virtual void ButtonPressedImpl(Button* sender) = 0;

  // Performs layout-specific update of the layout manager.
  virtual void UpdateLayoutManager() = 0;

 private:
  View* layout_panel_ = nullptr;
  View* control_panel_ = nullptr;
  LabelButton* add_button_ = nullptr;
  Textfield* preferred_width_view_ = nullptr;
  Textfield* preferred_height_view_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_LAYOUT_EXAMPLE_BASE_H_
