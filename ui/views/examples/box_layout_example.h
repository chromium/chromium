// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BOX_LAYOUT_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_BOX_LAYOUT_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/examples/layout_example_base.h"
#include "ui/views/layout/box_layout.h"

namespace views {

class Checkbox;
class Combobox;
class Textfield;

namespace examples {

class VIEWS_EXAMPLES_EXPORT BoxLayoutExample : public LayoutExampleBase {
 public:
  BoxLayoutExample();
  BoxLayoutExample(const BoxLayoutExample&) = delete;
  BoxLayoutExample& operator=(const BoxLayoutExample&) = delete;
  ~BoxLayoutExample() override;

 private:
  // LayoutExampleBase:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;
  void CreateAdditionalControls() override;
  void UpdateLayoutManager() override;

  // Set the border insets on the current BoxLayout instance.
  void UpdateBorderInsets();

  void MainAxisAlignmentChanged();
  void CrossAxisAlignmentChanged();

  raw_ptr<BoxLayout> layout_ = nullptr;
  raw_ptr<Combobox> orientation_ = nullptr;
  raw_ptr<Combobox> main_axis_alignment_ = nullptr;
  raw_ptr<Combobox> cross_axis_alignment_ = nullptr;
  raw_ptr<Textfield> between_child_spacing_ = nullptr;
  raw_ptr<Textfield> default_flex_ = nullptr;
  raw_ptr<Textfield> min_cross_axis_size_ = nullptr;
  InsetTextfields border_insets_;
  raw_ptr<Checkbox> collapse_margins_ = nullptr;
};

}  // namespace examples
}  // namespace views
#endif  // UI_VIEWS_EXAMPLES_BOX_LAYOUT_EXAMPLE_H_
