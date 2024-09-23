// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/slider_example.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace views::examples {

SliderExample::SliderExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_SLIDER_SELECT_LABEL).c_str()) {}

SliderExample::~SliderExample() = default;

void SliderExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(3), 3));

  auto* const container_default =
      container->AddChildView(std::make_unique<BoxLayoutView>());
  container_default->SetBetweenChildSpacing(3);
  std::u16string default_name =
      l10n_util::GetStringUTF16(IDS_SLIDER_DEFAULT_SLIDER_LABEL);
  label_default_ =
      container_default->AddChildView(std::make_unique<Label>(default_name));
  slider_default_ =
      container_default->AddChildView(std::make_unique<Slider>(this));
  slider_default_->SetValue(0.5);
  slider_default_->GetViewAccessibility().SetName(
      default_name, ax::mojom::NameFrom::kAttribute);

  auto* const container_minimal =
      container->AddChildView(std::make_unique<BoxLayoutView>());
  container_minimal->SetBetweenChildSpacing(3);
  std::u16string minimal_name =
      l10n_util::GetStringUTF16(IDS_SLIDER_MINIMAL_SLIDER_LABEL);
  label_minimal_ =
      container_minimal->AddChildView(std::make_unique<Label>(minimal_name));
  slider_minimal_ =
      container_minimal->AddChildView(std::make_unique<Slider>(this));
  slider_minimal_->SetValue(0.5);
  slider_minimal_->SetRenderingStyle(Slider::RenderingStyle::kMinimalStyle);
  slider_minimal_->GetViewAccessibility().SetName(
      minimal_name, ax::mojom::NameFrom::kAttribute);
}

void SliderExample::SliderValueChanged(Slider* sender,
                                       float value,
                                       float old_value,
                                       SliderChangeReason reason) {
  auto* const label =
      (sender == slider_default_) ? label_default_.get() : label_minimal_.get();
  label->SetText(base::ASCIIToUTF16(base::StringPrintf("%.3lf", value)));
}

}  // namespace views::examples
