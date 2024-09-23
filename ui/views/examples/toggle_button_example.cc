// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/toggle_button_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"

namespace views::examples {

constexpr int kLayoutInset = 8;

ToggleButtonExample::ToggleButtonExample()
    : ExampleBase(
          l10n_util::GetStringUTF8(IDS_TOGGLE_BUTTON_SELECT_LABEL).c_str()) {}

ToggleButtonExample::~ToggleButtonExample() = default;

void ToggleButtonExample::CreateExampleView(View* container) {
  auto layout =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical,
                                  gfx::Insets(kLayoutInset), kLayoutInset);
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(layout));
  container
      ->AddChildView(std::make_unique<ToggleButton>(base::BindRepeating(
          [](ToggleButtonExample* example) {
            PrintStatus("Pressed 1! count: %d", ++example->count_1_);
          },
          base::Unretained(this))))
      ->GetViewAccessibility()
      .SetName(l10n_util::GetStringUTF16(IDS_TOGGLE_BUTTON_NAME_1));
  auto* button = container->AddChildView(
      std::make_unique<ToggleButton>(base::BindRepeating(
          [](ToggleButtonExample* example) {
            PrintStatus("Pressed 2! count: %d", ++example->count_2_);
          },
          base::Unretained(this))));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TOGGLE_BUTTON_NAME_2));
  button->SetIsOn(true);
  button = container->AddChildView(
      std::make_unique<ToggleButton>(base::BindRepeating(
          [](ToggleButtonExample* example) {
            PrintStatus("Pressed 3! count: %d", ++example->count_2_);
          },
          base::Unretained(this))));
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TOGGLE_BUTTON_NAME_3));
  button->SetEnabled(false);
}

}  // namespace views::examples
