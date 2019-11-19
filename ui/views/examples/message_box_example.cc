// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/message_box_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

MessageBoxExample::MessageBoxExample() : ExampleBase("Message Box View") {
}

MessageBoxExample::~MessageBoxExample() = default;

void MessageBoxExample::CreateExampleView(View* container) {
  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  auto message_box_view = std::make_unique<MessageBoxView>(
      MessageBoxView::InitParams(ASCIIToUTF16("Hello, world!")));
  message_box_view->SetCheckBoxLabel(ASCIIToUTF16("Check Box"));

  const int message_box_column = 0;
  ColumnSet* column_set = layout->AddColumnSet(message_box_column);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1 /* expand */, message_box_column);
  message_box_view_ = layout->AddView(std::move(message_box_view));

  const int button_column = 1;
  column_set = layout->AddColumnSet(button_column);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        0.5f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        0.5f, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0 /* no expand */, button_column);

  status_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Show Status")));
  toggle_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Toggle Checkbox")));
}

void MessageBoxExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == status_) {
    message_box_view_->SetCheckBoxLabel(
        ASCIIToUTF16(message_box_view_->IsCheckBoxSelected() ? "on" : "off"));
    PrintStatus(message_box_view_->IsCheckBoxSelected() ?
       "Check Box Selected" : "Check Box Not Selected");
  } else if (sender == toggle_) {
    message_box_view_->SetCheckBoxSelected(
        !message_box_view_->IsCheckBoxSelected());
  }
}

}  // namespace examples
}  // namespace views
