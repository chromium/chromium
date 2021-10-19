// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/toggle_button_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace examples {

ToggleButtonExample::ToggleButtonExample()
    : ExampleBase(
          l10n_util::GetStringUTF8(IDS_TOGGLE_BUTTON_SELECT_LABEL).c_str()) {}

ToggleButtonExample::~ToggleButtonExample() = default;

void ToggleButtonExample::CreateExampleView(View* container) {
  auto layout = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(layout));

  auto toggle_button1 = std::make_unique<ToggleButton>(base::BindRepeating(
      [](ToggleButtonExample* example) {
        PrintStatus("Pressed 1! count: %d", ++example->count_1_);
      },
      base::Unretained(this)));
  // TODO(pbos): Figure out a reasonable accessible name here.
  toggle_button1->SetAccessibleName(u"TODO: Add a reasonable Accessible Name");

  auto toggle_button2 = std::make_unique<ToggleButton>(base::BindRepeating(
      [](ToggleButtonExample* example) {
        PrintStatus("Pressed 2! count: %d", ++example->count_2_);
      },
      base::Unretained(this)));
  toggle_button2->SetIsOn(true);
  // TODO(pbos): Figure out a reasonable accessible name here.
  toggle_button2->SetAccessibleName(u"TODO: Add a reasonable Accessible Name");

  container->AddChildView(std::move(toggle_button1));
  container->AddChildView(std::move(toggle_button2));
}

}  // namespace examples
}  // namespace views
