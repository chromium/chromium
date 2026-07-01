// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/flex_layout_multiple_order_example.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views::examples {

namespace {

// Predicate that defines when a FlexSpecification should use a higher order.
bool EnableHighOrderRule(const SizeBounds& size_bounds) {
  return size_bounds.width().is_bounded() && size_bounds.width().value() >= 200;
}

}  // namespace

FlexLayoutMultipleOrderExample::UpdateChildrenOnLayoutView::
    UpdateChildrenOnLayoutView(
        base::WeakPtr<FlexLayoutMultipleOrderExample> parent)
    : parent_(parent) {}

FlexLayoutMultipleOrderExample::UpdateChildrenOnLayoutView::
    ~UpdateChildrenOnLayoutView() = default;

void FlexLayoutMultipleOrderExample::UpdateChildrenOnLayoutView::Layout(
    PassKey) {
  // Call super implementation to ensure layout manager and child layouts
  // happen.
  LayoutSuperclass<View>(this);
  if (parent_) {
    parent_->UpdateViews();
  }
}

BEGIN_METADATA(FlexLayoutMultipleOrderExample, UpdateChildrenOnLayoutView)
END_METADATA

FlexLayoutMultipleOrderExample::FlexLayoutMultipleOrderExample()
    : ExampleBase("Flex Layout Multiple Order") {}

FlexLayoutMultipleOrderExample::~FlexLayoutMultipleOrderExample() = default;

void FlexLayoutMultipleOrderExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));

  // Status label
  status_label_ = container->AddChildView(std::make_unique<Label>());
  status_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Borders & containment panel
  auto* border_view = container->AddChildView(std::make_unique<View>());
  BoxLayout* border_layout =
      border_view->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::Orientation::kVertical, gfx::Insets(5)));
  border_layout->set_cross_axis_alignment(
      BoxLayout::CrossAxisAlignment::kStart);

  // The actual flex container view.
  flex_container_ = border_view->AddChildView(std::make_unique<View>());
  flex_container_->SetBorder(CreateSolidBorder(1, gfx::kGoogleGrey300));
  flex_container_->SetBackground(CreateSolidBackground(gfx::kGoogleGrey100));
  auto* flex_layout =
      flex_container_->SetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_container_->SetPreferredSize(gfx::Size(800, 40));

  // Child View 1 (orders={3,1}, preferred=200, intermediate min=100, final
  // min=0)
  view1_ = flex_container_->AddChildView(
      std::make_unique<UpdateChildrenOnLayoutView>(
          weak_ptr_factory_.GetWeakPtr()));
  view1_->SetBorder(CreateSolidBorder(2, gfx::kGoogleBlue500));
  auto* view1_layout = view1_->SetLayoutManager(std::make_unique<FlexLayout>());
  view1_layout->SetOrientation(LayoutOrientation::kHorizontal);

  // View A inside View 1 (order=1, preferred=100, snaps to zero, weight 0)
  view_a_ = view1_->AddChildView(std::make_unique<View>());
  view_a_->SetBackground(CreateSolidBackground(SK_ColorBLUE));
  view_a_->SetLayoutManager(std::make_unique<FillLayout>());
  view_a_label_ = view_a_->AddChildView(std::make_unique<Label>(u"A"));
  view_a_label_->SetEnabledColor(SK_ColorWHITE);
  view_a_->SetPreferredSize(gfx::Size(100, 30));
  view_a_->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(LayoutOrientation::kHorizontal,
                        MinimumFlexSizeRule::kPreferredSnapToZero,
                        MaximumFlexSizeRule::kPreferred)
          .WithOrder(1)
          .WithWeight(0));

  // View B inside View 1 (order=3, preferred=100, snaps to zero, weight 0)
  view_b_ = view1_->AddChildView(std::make_unique<View>());
  view_b_->SetBackground(CreateSolidBackground(SK_ColorCYAN));
  view_b_->SetLayoutManager(std::make_unique<FillLayout>());
  view_b_label_ = view_b_->AddChildView(std::make_unique<Label>(u"B"));
  view_b_label_->SetEnabledColor(SK_ColorBLACK);
  view_b_->SetPreferredSize(gfx::Size(100, 30));
  view_b_->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(LayoutOrientation::kHorizontal,
                        MinimumFlexSizeRule::kPreferredSnapToZero,
                        MaximumFlexSizeRule::kPreferred)
          .WithOrder(3)
          .WithWeight(0));

  // Set up multiple rules and predicates for View 1.
  FlexRule default_rule_1 = view1_layout->GetDefaultFlexRule();
  std::vector<RuleAndPredicate> rules_and_predicates_1;
  rules_and_predicates_1.emplace_back(
      3, default_rule_1, base::BindRepeating(&EnableHighOrderRule));
  rules_and_predicates_1.emplace_back(1, default_rule_1,
                                      views::RuleEnabledPredicate());
  view1_->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(std::move(rules_and_predicates_1)).WithWeight(0));

  // Spacer View (order=1, weight=1, takes remaining width)
  spacer_ = flex_container_->AddChildView(std::make_unique<View>());
  spacer_->SetBackground(CreateSolidBackground(gfx::kGoogleGreen300));
  spacer_->SetLayoutManager(std::make_unique<FillLayout>());
  spacer_label_ = spacer_->AddChildView(std::make_unique<Label>(u"Spacer"));
  spacer_label_->SetEnabledColor(SK_ColorWHITE);
  spacer_->SetPreferredSize(gfx::Size(300, 30));
  spacer_->SetProperty(views::kFlexBehaviorKey,
                       FlexSpecification(MinimumFlexSizeRule::kScaleToMinimum,
                                         MaximumFlexSizeRule::kUnbounded)
                           .WithWeight(1)
                           .WithOrder(1));

  // Child View 2 (orders={4,2}, preferred=200, intermediate min=100, final
  // min=0)
  view2_ = flex_container_->AddChildView(std::make_unique<View>());
  view2_->SetBorder(CreateSolidBorder(2, gfx::kGoogleRed500));
  auto* view2_layout = view2_->SetLayoutManager(std::make_unique<FlexLayout>());
  view2_layout->SetOrientation(LayoutOrientation::kHorizontal);

  // View C inside View 2 (order=2, preferred=100, snaps to zero, weight 0)
  view_c_ = view2_->AddChildView(std::make_unique<View>());
  view_c_->SetBackground(CreateSolidBackground(SK_ColorRED));
  view_c_->SetLayoutManager(std::make_unique<FillLayout>());
  view_c_label_ = view_c_->AddChildView(std::make_unique<Label>(u"C"));
  view_c_label_->SetEnabledColor(SK_ColorWHITE);
  view_c_->SetPreferredSize(gfx::Size(100, 30));
  view_c_->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(LayoutOrientation::kHorizontal,
                        MinimumFlexSizeRule::kPreferredSnapToZero,
                        MaximumFlexSizeRule::kPreferred)
          .WithOrder(2)
          .WithWeight(0));

  // View D inside View 2 (order=4, preferred=100, snaps to zero, weight 0)
  view_d_ = view2_->AddChildView(std::make_unique<View>());
  view_d_->SetBackground(CreateSolidBackground(SK_ColorYELLOW));
  view_d_->SetLayoutManager(std::make_unique<FillLayout>());
  view_d_label_ = view_d_->AddChildView(std::make_unique<Label>(u"D"));
  view_d_label_->SetEnabledColor(SK_ColorBLACK);
  view_d_->SetPreferredSize(gfx::Size(100, 30));
  view_d_->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(LayoutOrientation::kHorizontal,
                        MinimumFlexSizeRule::kPreferredSnapToZero,
                        MaximumFlexSizeRule::kPreferred)
          .WithOrder(4)
          .WithWeight(0));

  // Set up multiple rules and predicates for View 2
  FlexRule default_rule_2 = view2_layout->GetDefaultFlexRule();
  std::vector<RuleAndPredicate> rules_and_predicates_2;
  rules_and_predicates_2.emplace_back(
      4, default_rule_2, base::BindRepeating(&EnableHighOrderRule));
  rules_and_predicates_2.emplace_back(2, default_rule_2,
                                      views::RuleEnabledPredicate());
  view2_->SetProperty(
      views::kFlexBehaviorKey,
      FlexSpecification(std::move(rules_and_predicates_2)).WithWeight(0));

  // Instructions label
  auto* instructions = container->AddChildView(std::make_unique<Label>());
  instructions->SetMultiLine(true);
  instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  instructions->SetText(
      u"Resize the window to resize the flex container.\n"
      u"• View 1 (blue border) has nested View A (order 1) & View B (order "
      u"3).\n"
      u"• View 2 (red border) has nested View C (order 2) & View D (order 4).\n"
      u"• A green Spacer (minimum 300 DIP) sits between View 1 and View 2.\n"
      u"• Subviews (A, B, C, D) have weight 0 and snap to zero or take "
      u"preferred width (100 DIP).\n"
      u"As container shrinks below 708 DIP, the Spacer drops to its minimum "
      u"300 DIP and stops.\n"
      u"At smaller widths, views snap to zero:\n"
      u"- View D snaps to zero first (Order 4: highest active order).\n"
      u"- View B snaps to zero second (Order 3: highest active after Order 4)."
      u".\n"
      u"- View C snaps to zero third (Order 2: highest active after Order 3)."
      u"\n"
      u"- View A snaps to zero last (Order 1: last active order).\n"
      u"The green Spacer expands to occupy any remaining space when views "
      u"snap.");

  UpdateViews();
}

void FlexLayoutMultipleOrderExample::UpdateViews() {
  // Draw status update
  int width = flex_container_->width();
  int w1 = view1_->width();
  int w2 = view2_->width();
  int wsp = spacer_->width();
  int wa = view_a_->width();
  int wb = view_b_->width();
  int wc = view_c_->width();
  int wd = view_d_->width();

  status_label_->SetText(base::ASCIIToUTF16(base::StringPrintf(
      "Width: %d | V1: %d (A=%d, B=%d) | Spacer: %d | V2: %d (C=%d, D=%d)",
      width, w1, wa, wb, wsp, w2, wc, wd)));

  view_a_label_->SetText(
      base::ASCIIToUTF16(base::StringPrintf("A (w=%d)", wa)));
  view_b_label_->SetText(
      base::ASCIIToUTF16(base::StringPrintf("B (w=%d)", wb)));
  spacer_label_->SetText(
      base::ASCIIToUTF16(base::StringPrintf("Spacer (w=%d)", wsp)));
  view_c_label_->SetText(
      base::ASCIIToUTF16(base::StringPrintf("C (w=%d)", wc)));
  view_d_label_->SetText(
      base::ASCIIToUTF16(base::StringPrintf("D (w=%d)", wd)));
}

}  // namespace views::examples
