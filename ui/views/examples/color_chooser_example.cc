// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/color_chooser_example.h"

#include "ui/views/widget/widget_delegate.h"

namespace views::examples {

ColorChooserExample::ColorChooserExample() : ExampleBase("ColorChooser") {}

ColorChooserExample::~ColorChooserExample() = default;

void ColorChooserExample::CreateExampleView(View* container) {
  container->SetUseDefaultFillLayout(true);
  container->AddChildView(
      chooser_.MakeWidgetDelegate()->TransferOwnershipOfContentsView());
}

void ColorChooserExample::OnColorChosen(SkColor color) {}

void ColorChooserExample::OnColorChooserDialogClosed() {}

}  // namespace views::examples
