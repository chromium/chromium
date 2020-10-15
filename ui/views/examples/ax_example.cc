// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/ax_example.h"

#include <memory>

#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace views {
namespace examples {

AxExample::AxExample() : ExampleBase("Accessibility Features") {}

AxExample::~AxExample() = default;

void AxExample::CreateExampleView(View* container) {
  container->SetBackground(CreateSolidBackground(SK_ColorWHITE));
  FlexLayout* const layout =
      container->SetLayoutManager(std::make_unique<FlexLayout>());
  layout->SetCollapseMargins(true);
  layout->SetOrientation(LayoutOrientation::kVertical);
  layout->SetDefault(kMarginsKey, gfx::Insets(10));
  layout->SetMainAxisAlignment(LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(LayoutAlignment::kStart);

  auto announce_text = [](AxExample* example) {
    example->announce_button_->GetViewAccessibility().AnnounceText(
        base::ASCIIToUTF16("Button pressed."));
  };

  announce_button_ = container->AddChildView(std::make_unique<MdTextButton>(
      base::BindRepeating(announce_text, base::Unretained(this)),
      base::ASCIIToUTF16("AnnounceText")));
}

}  // namespace examples
}  // namespace views
