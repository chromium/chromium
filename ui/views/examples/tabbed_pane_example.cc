// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/tabbed_pane_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

TabbedPaneExample::TabbedPaneExample()
    : ExampleBase(GetStringUTF8(IDS_TABBED_PANE_SELECT_LABEL).c_str()) {}

TabbedPaneExample::~TabbedPaneExample() = default;

void TabbedPaneExample::CreateExampleView(View* container) {
  auto tabbed_pane = std::make_unique<TabbedPane>();
  tabbed_pane->set_listener(this);
  auto add = std::make_unique<LabelButton>(
      base::BindRepeating(&TabbedPaneExample::AddButton, base::Unretained(this),
                          GetStringUTF16(IDS_TABBED_PANE_ADDED_LABEL)),
      GetStringUTF16(IDS_TABBED_PANE_ADD_LABEL));

  auto add_at = std::make_unique<LabelButton>(
      base::BindRepeating(&TabbedPaneExample::AddAtButtonPressed,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TABBED_PANE_ADD_1_LABEL));
  auto select_at = std::make_unique<LabelButton>(
      base::BindRepeating(
          [](TabbedPane* pane) {
            if (pane->GetTabCount() > 1)
              pane->SelectTabAt(1);
          },
          base::Unretained(tabbed_pane_)),
      GetStringUTF16(IDS_TABBED_PANE_SELECT_1_LABEL));

  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(LayoutOrientation::kVertical);

  auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                     MaximumFlexSizeRule::kUnbounded)
                       .WithWeight(1);

  tabbed_pane_ = container->AddChildView(std::move(tabbed_pane));
  tabbed_pane_->SetProperty(views::kFlexBehaviorKey, full_flex);

  // Create a few tabs with a button first.
  AddButton(GetStringUTF16(IDS_TABBED_PANE_TAB_1_LABEL));
  AddButton(GetStringUTF16(IDS_TABBED_PANE_TAB_2_LABEL));
  AddButton(GetStringUTF16(IDS_TABBED_PANE_TAB_3_LABEL));

  // Add control buttons horizontally.
  auto* button_panel = container->AddChildView(std::make_unique<View>());
  button_panel->SetLayoutManager(std::make_unique<views::FlexLayout>());
  add_ = button_panel->AddChildView(std::move(add));
  add_at_ = button_panel->AddChildView(std::move(add_at));
  select_at_ = button_panel->AddChildView(std::move(select_at));

  for (View* view : button_panel->children())
    view->SetProperty(views::kFlexBehaviorKey, full_flex);
}

void TabbedPaneExample::TabSelectedAt(int index) {
  // Just print the status when selection changes.
  PrintCurrentStatus();
}

void TabbedPaneExample::PrintCurrentStatus() {
  PrintStatus("Tab Count:%" PRIuS ", Selected Tab:%" PRIuS,
              tabbed_pane_->GetTabCount(), tabbed_pane_->GetSelectedTabIndex());
}

void TabbedPaneExample::AddButton(const base::string16& label) {
  tabbed_pane_->AddTab(
      label, std::make_unique<LabelButton>(Button::PressedCallback(), label));
  PrintCurrentStatus();
}

void TabbedPaneExample::AddAtButtonPressed() {
  const base::string16 label = GetStringUTF16(IDS_TABBED_PANE_ADDED_1_LABEL);
  tabbed_pane_->AddTabAtIndex(
      1, label,
      std::make_unique<LabelButton>(Button::PressedCallback(), label));
  PrintCurrentStatus();
}

}  // namespace examples
}  // namespace views
