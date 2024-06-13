// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_LABEL_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_LABEL_EXAMPLE_H_

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view_observer.h"

namespace views {

class Checkbox;
class Combobox;
class Label;
class View;

namespace examples {

class VIEWS_EXAMPLES_EXPORT LabelExample : public ExampleBase,
                                           public TextfieldController,
                                           public ViewObserver {
 public:
  LabelExample();

  LabelExample(const LabelExample&) = delete;
  LabelExample& operator=(const LabelExample&) = delete;

  ~LabelExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  void MultilineCheckboxPressed();
  void ShadowsCheckboxPressed();
  void SelectableCheckboxPressed();

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;

  // ViewObserver:
  void OnViewThemeChanged(View* observed_view) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  // Add a customizable label and various controls to modify its presentation.
  void AddCustomLabel(View* container);

  // Creates and adds a combobox to the layout.
  Combobox* AddCombobox(View* parent,
                        std::u16string name,
                        base::span<const char* const> items,
                        void (LabelExample::*function)());

  void AlignmentChanged();
  void ElidingChanged();

  raw_ptr<Textfield> textfield_ = nullptr;
  raw_ptr<Combobox> alignment_ = nullptr;
  raw_ptr<Combobox> elide_behavior_ = nullptr;
  raw_ptr<Checkbox> multiline_ = nullptr;
  raw_ptr<Checkbox> shadows_ = nullptr;
  raw_ptr<Checkbox> selectable_ = nullptr;
  raw_ptr<Label> label_ = nullptr;
  raw_ptr<Label> custom_label_ = nullptr;

  base::ScopedObservation<View, ViewObserver> observer_{this};
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_LABEL_EXAMPLE_H_
