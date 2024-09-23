// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/accessibility_paint_checks.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

using AccessibilityPaintChecksTest = ViewsTestBase;

// Test that a view that is not accessible will fail the accessibility audit.
TEST_F(AccessibilityPaintChecksTest, VerifyAccessibilityCheckerFailAndPass) {
  // Create containing widget.
  Widget widget;
  Widget::InitParams params = Widget::InitParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 650, 650);
  params.context = GetContext();
  widget.Init(std::move(params));
  widget.Show();

  // Add the button.
  auto* button =
      widget.GetContentsView()->AddChildView(std::make_unique<ImageButton>());

  // Accessibility test should pass as it is focusable but has a name.
  button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button->GetViewAccessibility().SetName(u"Some name");
  RunAccessibilityPaintChecks(&widget);

  // Accessibility test should pass as it has no name but is not focusable.
  button->SetFocusBehavior(View::FocusBehavior::NEVER);
  button->GetViewAccessibility().SetName(u"");
  RunAccessibilityPaintChecks(&widget);

  // Accessibility test should fail as it has no name and is focusable.
  button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_DCHECK_DEATH_WITH(RunAccessibilityPaintChecks(&widget), "name");

  // Restore the name of the button so that it is not the source of failure.
  button->GetViewAccessibility().SetName(u"Some name");

  // Accessibility test should fail if the focusable view lacks a valid role.
  auto* generic_view =
      widget.GetContentsView()->AddChildView(std::make_unique<View>());
  generic_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_DCHECK_DEATH_WITH(RunAccessibilityPaintChecks(&widget), "role");
}

}  // namespace views
