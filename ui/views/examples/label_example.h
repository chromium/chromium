// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_LABEL_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_LABEL_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Checkbox;
class Combobox;
class GridLayout;
class Label;

namespace examples {

class VIEWS_EXAMPLES_EXPORT LabelExample : public ExampleBase,
                                           public TextfieldController {
 public:
  LabelExample();
  ~LabelExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  void MultilineCheckboxPressed();
  void ShadowsCheckboxPressed();
  void SelectableCheckboxPressed();

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;

 private:
  // Add a customizable label and various controls to modify its presentation.
  void AddCustomLabel(View* container);

  // Creates and adds a combobox to the layout.
  Combobox* AddCombobox(GridLayout* layout,
                        const char* name,
                        const char** strings,
                        int count,
                        void (LabelExample::*function)());

  void AlignmentChanged();
  void ElidingChanged();

  Textfield* textfield_ = nullptr;
  Combobox* alignment_ = nullptr;
  Combobox* elide_behavior_ = nullptr;
  Checkbox* multiline_ = nullptr;
  Checkbox* shadows_ = nullptr;
  Checkbox* selectable_ = nullptr;
  Label* custom_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LabelExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_LABEL_EXAMPLE_H_
