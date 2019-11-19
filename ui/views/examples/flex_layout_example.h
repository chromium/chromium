// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_FLEX_LAYOUT_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_FLEX_LAYOUT_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/layout_example_base.h"
#include "ui/views/layout/flex_layout.h"

namespace views {

class Checkbox;
class Combobox;
class Textfield;

namespace examples {

class VIEWS_EXAMPLES_EXPORT FlexLayoutExample : public LayoutExampleBase {
 public:
  FlexLayoutExample();
  ~FlexLayoutExample() override;

 private:
  // ComboboxListener
  void OnPerformAction(Combobox* combobox) override;

  // TextfieldController
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;

  // LayoutExampleBase
  void ButtonPressedImpl(Button* sender) override;
  void CreateAdditionalControls(int vertical_start_pos) override;
  void UpdateLayoutManager() override;

  FlexSpecification GetFlexSpecification(int weight) const;

  FlexLayout* layout_ = nullptr;
  Combobox* orientation_ = nullptr;
  Combobox* main_axis_alignment_ = nullptr;
  Combobox* cross_axis_alignment_ = nullptr;
  Textfield* between_child_spacing_ = nullptr;
  Checkbox* collapse_margins_ = nullptr;
  InsetTextfields interior_margin_;
  InsetTextfields default_child_margins_;
  Checkbox* ignore_default_main_axis_margins_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FlexLayoutExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_FLEX_LAYOUT_EXAMPLE_H_
