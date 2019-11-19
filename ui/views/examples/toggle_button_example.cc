// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/toggle_button_example.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace examples {

ToggleButtonExample::ToggleButtonExample() : ExampleBase("Toggle button") {}

ToggleButtonExample::~ToggleButtonExample() = default;

void ToggleButtonExample::CreateExampleView(View* container) {
  button_ = new ToggleButton(this);
  auto layout = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(layout));
  container->AddChildView(button_);
}

void ToggleButtonExample::ButtonPressed(Button* sender,
                                        const ui::Event& event) {
  PrintStatus("Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
