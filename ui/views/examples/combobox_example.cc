// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/combobox_example.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace views::examples {

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
  size_t GetItemCount() const override { return 10; }
  std::u16string GetItemAt(size_t index) const override {
    return base::UTF8ToUTF16(
        base::StringPrintf("%c item", static_cast<char>('A' + index)));
  }
};

}  // namespace

ComboboxExample::ComboboxExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_COMBOBOX_SELECT_LABEL).c_str()) {
}

ComboboxExample::~ComboboxExample() = default;

void ComboboxExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());

  auto view =
      Builder<BoxLayoutView>()
          .SetOrientation(BoxLayout::Orientation::kVertical)
          .SetInsideBorderInsets(gfx::Insets::VH(10, 0))
          .SetBetweenChildSpacing(5)
          .AddChildren(
              Builder<Combobox>()
                  .CopyAddressTo(&combobox_)
                  .SetOwnedModel(std::make_unique<ComboboxModelExample>())
                  .SetSelectedIndex(3)
                  .SetAccessibleName(
                      l10n_util::GetStringUTF16(IDS_COMBOBOX_NAME_1))
                  .SetCallback(base::BindRepeating(
                      &ComboboxExample::ValueChanged, base::Unretained(this))),
              Builder<Combobox>()
                  .SetOwnedModel(std::make_unique<ComboboxModelExample>())
                  .SetEnabled(false)
                  .SetSelectedIndex(4)
                  .SetAccessibleName(
                      l10n_util::GetStringUTF16(IDS_COMBOBOX_NAME_2))
                  .SetCallback(base::BindRepeating(
                      &ComboboxExample::ValueChanged, base::Unretained(this))),
              Builder<EditableCombobox>()
                  .CopyAddressTo(&editable_combobox_)
                  .SetModel(std::make_unique<ComboboxModelExample>())
                  .SetEnabled(true)
                  .SetAccessibleName(
                      l10n_util::GetStringUTF16(IDS_COMBOBOX_NAME_2))
                  .SetCallback(base::BindRepeating(
                      &ComboboxExample::EditableValueChanged,
                      base::Unretained(this))))
          .Build();

  container->AddChildView(std::move(view));
}

void ComboboxExample::ValueChanged() {
  PrintStatus("Selected: %s",
              base::UTF16ToUTF8(combobox_->GetModel()->GetItemAt(
                                    combobox_->GetSelectedIndex().value()))
                  .c_str());
}

void ComboboxExample::EditableValueChanged() {
  PrintStatus("Changed: %s",
              base::UTF16ToUTF8(editable_combobox_->GetText()).c_str());
}

}  // namespace views::examples
