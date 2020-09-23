// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace examples {

namespace {

// A combobox model implementation that generates a list of "Item <index>".
class ComboboxModelExample : public ui::ComboboxModel {
 public:
  ComboboxModelExample() = default;
  ComboboxModelExample(const ComboboxModelExample&) = delete;
  ComboboxModelExample& operator=(const ComboboxModelExample&) = delete;
  ~ComboboxModelExample() override = default;

 private:
  // ui::ComboboxModel:
  int GetItemCount() const override { return 10; }
  base::string16 GetItemAt(int index) const override {
    return base::UTF8ToUTF16(base::StringPrintf("%c item", 'A' + index));
  }
};

}  // namespace

ComboboxExample::ComboboxExample() : ExampleBase("Combo Box") {}

ComboboxExample::~ComboboxExample() = default;

void ComboboxExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10, 0), 5));

  combobox_ = container->AddChildView(
      std::make_unique<Combobox>(std::make_unique<ComboboxModelExample>()));
  combobox_->set_callback(base::BindRepeating(&ComboboxExample::ValueChanged,
                                              base::Unretained(this)));
  combobox_->SetSelectedIndex(3);

  auto* disabled_combobox = container->AddChildView(
      std::make_unique<Combobox>(std::make_unique<ComboboxModelExample>()));
  disabled_combobox->set_callback(base::BindRepeating(
      &ComboboxExample::ValueChanged, base::Unretained(this)));
  disabled_combobox->SetSelectedIndex(4);
  disabled_combobox->SetEnabled(false);
}

void ComboboxExample::ValueChanged() {
  PrintStatus("Selected: %s",
              base::UTF16ToUTF8(
                  combobox_->model()->GetItemAt(combobox_->GetSelectedIndex()))
                  .c_str());
}

}  // namespace examples
}  // namespace views
