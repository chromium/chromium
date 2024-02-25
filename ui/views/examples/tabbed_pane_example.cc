// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/tabbed_pane_example.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
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

namespace views::examples {

TabbedPaneExample::TabbedPaneExample()
    : ExampleBase(GetStringUTF8(IDS_TABBED_PANE_SELECT_LABEL).c_str()) {}

TabbedPaneExample::~TabbedPaneExample() {
  if (tabbed_pane_) {
    tabbed_pane_->set_listener(nullptr);
  }
}

void TabbedPaneExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(LayoutOrientation::kVertical);

  // Add control buttons horizontally.
  auto* const button_panel = container->AddChildView(std::make_unique<View>());
  button_panel->SetLayoutManager(std::make_unique<views::FlexLayout>());
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TabbedPaneExample::SwapLayout,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TABBED_PANE_SWAP_LAYOUT_LABEL)));
  toggle_highlighted_ =
      button_panel->AddChildView(std::make_unique<LabelButton>(
          base::BindRepeating(&TabbedPaneExample::ToggleHighlighted,
                              base::Unretained(this)),
          GetStringUTF16(IDS_TABBED_PANE_TOGGLE_HIGHLIGHTED_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TabbedPaneExample::AddTab, base::Unretained(this),
                          GetStringUTF16(IDS_TABBED_PANE_ADDED_LABEL)),
      GetStringUTF16(IDS_TABBED_PANE_ADD_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TabbedPaneExample::AddAt, base::Unretained(this)),
      GetStringUTF16(IDS_TABBED_PANE_ADD_1_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TabbedPaneExample::SelectAt, base::Unretained(this)),
      GetStringUTF16(IDS_TABBED_PANE_SELECT_1_LABEL)));

  const auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                           MaximumFlexSizeRule::kUnbounded)
                             .WithWeight(1);
  for (View* view : button_panel->children())
    view->SetProperty(views::kFlexBehaviorKey, full_flex);

  CreateTabbedPane(container, TabbedPane::Orientation::kHorizontal,
                   TabbedPane::TabStripStyle::kBorder);
}

void TabbedPaneExample::TabSelectedAt(int index) {
  // Just print the status when selection changes.
  PrintCurrentStatus();
}

void TabbedPaneExample::CreateTabbedPane(View* container,
                                         TabbedPane::Orientation orientation,
                                         TabbedPane::TabStripStyle style) {
  // Tabbed panes only support highlighted style for vertical tabs.
  if (orientation == TabbedPane::Orientation::kHorizontal)
    style = TabbedPane::TabStripStyle::kBorder;

  tabbed_pane_ = container->AddChildViewAt(
      std::make_unique<TabbedPane>(orientation, style), 0);
  tabbed_pane_->set_listener(this);
  toggle_highlighted_->SetEnabled(orientation ==
                                  TabbedPane::Orientation::kVertical);
  const auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                           MaximumFlexSizeRule::kUnbounded)
                             .WithWeight(1);
  tabbed_pane_->SetProperty(views::kFlexBehaviorKey, full_flex);
  AddTab(GetStringUTF16(IDS_TABBED_PANE_TAB_1_LABEL));
  AddTab(GetStringUTF16(IDS_TABBED_PANE_TAB_2_LABEL));
  AddTab(GetStringUTF16(IDS_TABBED_PANE_TAB_3_LABEL));
}

void TabbedPaneExample::PrintCurrentStatus() {
  PrintStatus("Tab Count:%" PRIuS ", Selected Tab:%" PRIuS,
              tabbed_pane_->GetTabCount(), tabbed_pane_->GetSelectedTabIndex());
}

void TabbedPaneExample::SwapLayout() {
  auto* const container = tabbed_pane_->parent();
  const auto orientation =
      (tabbed_pane_->GetOrientation() == TabbedPane::Orientation::kHorizontal)
          ? TabbedPane::Orientation::kVertical
          : TabbedPane::Orientation::kHorizontal;
  const auto style = tabbed_pane_->GetStyle();
  container->RemoveChildView(tabbed_pane_);
  CreateTabbedPane(container, orientation, style);
}

void TabbedPaneExample::ToggleHighlighted() {
  auto* const container = tabbed_pane_->parent();
  const auto orientation = tabbed_pane_->GetOrientation();
  const auto style =
      (tabbed_pane_->GetStyle() == TabbedPane::TabStripStyle::kBorder)
          ? TabbedPane::TabStripStyle::kHighlight
          : TabbedPane::TabStripStyle::kBorder;
  container->RemoveChildView(tabbed_pane_);
  CreateTabbedPane(container, orientation, style);
}

void TabbedPaneExample::AddTab(const std::u16string& label) {
  tabbed_pane_->AddTab(
      label, std::make_unique<LabelButton>(Button::PressedCallback(), label));
  PrintCurrentStatus();
}

void TabbedPaneExample::AddAt() {
  const std::u16string label = GetStringUTF16(IDS_TABBED_PANE_ADDED_1_LABEL);
  tabbed_pane_->AddTabAtIndex(
      1, label,
      std::make_unique<LabelButton>(Button::PressedCallback(), label));
  PrintCurrentStatus();
}

void TabbedPaneExample::SelectAt() {
  if (tabbed_pane_->GetTabCount() > 1)
    tabbed_pane_->SelectTabAt(1);
}

}  // namespace views::examples
