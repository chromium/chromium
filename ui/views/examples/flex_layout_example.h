// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_FLEX_LAYOUT_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_FLEX_LAYOUT_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
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
  FlexLayoutExample(const FlexLayoutExample&) = delete;
  FlexLayoutExample& operator=(const FlexLayoutExample&) = delete;
  ~FlexLayoutExample() override;

 private:
  // LayoutExampleBase:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;
  void CreateAdditionalControls() override;
  void UpdateLayoutManager() override;

  FlexSpecification GetFlexSpecification(int weight) const;

  void OrientationChanged();
  void MainAxisAlignmentChanged();
  void CrossAxisAlignmentChanged();

  raw_ptr<FlexLayout> layout_ = nullptr;
  raw_ptr<Combobox> orientation_ = nullptr;
  raw_ptr<Combobox> main_axis_alignment_ = nullptr;
  raw_ptr<Combobox> cross_axis_alignment_ = nullptr;
  raw_ptr<Checkbox> collapse_margins_ = nullptr;
  InsetTextfields interior_margin_;
  InsetTextfields default_child_margins_;
  raw_ptr<Checkbox> ignore_default_main_axis_margins_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_FLEX_LAYOUT_EXAMPLE_H_
