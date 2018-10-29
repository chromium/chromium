// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace examples {

namespace {

// A combobox model implementation that generates a list of "Item <index>".
class ComboboxModelExample : public ui::ComboboxModel {
 public:
  ComboboxModelExample() = default;
  ~ComboboxModelExample() override = default;

 private:
  // ui::ComboboxModel:
  int GetItemCount() const override { return 10; }
  base::string16 GetItemAt(int index) override {
    return base::UTF8ToUTF16(base::StringPrintf("%c item", 'A' + index));
  }

  DISALLOW_COPY_AND_ASSIGN(ComboboxModelExample);
};

}  // namespace

ComboboxExample::ComboboxExample() : ExampleBase("Combo Box") {
}

ComboboxExample::~ComboboxExample() = default;

void ComboboxExample::CreateExampleView(View* container) {
  combobox_ = new Combobox(std::make_unique<ComboboxModelExample>());
  combobox_->set_listener(this);
  combobox_->SetSelectedIndex(3);

  disabled_combobox_ = new Combobox(std::make_unique<ComboboxModelExample>());
  disabled_combobox_->set_listener(this);
  disabled_combobox_->SetSelectedIndex(4);
  disabled_combobox_->SetEnabled(false);

  container->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::kVertical, gfx::Insets(10, 0), 5));
  container->AddChildView(combobox_);
  container->AddChildView(disabled_combobox_);
}

void ComboboxExample::OnPerformAction(Combobox* combobox) {
  DCHECK_EQ(combobox, combobox_);
  PrintStatus("Selected: %s", base::UTF16ToUTF8(combobox->model()->GetItemAt(
                                                    combobox->selected_index()))
                                  .c_str());
}

}  // namespace examples
}  // namespace views
