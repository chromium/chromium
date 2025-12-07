// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/null_ax_action_target.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

TEST(NullAXActionTargetTest, TestMethods) {
  std::unique_ptr<AXActionTarget> action_target =
      std::make_unique<NullAXActionTarget>();

  EXPECT_EQ(AXActionTarget::Type::kNull, action_target->GetType());
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  EXPECT_FALSE(action_target->PerformAction(action_data));
  EXPECT_EQ(gfx::Rect(), action_target->GetRelativeBounds());
  EXPECT_EQ(gfx::Point(), action_target->GetScrollOffset());
  EXPECT_EQ(gfx::Point(), action_target->MinimumScrollOffset());
  EXPECT_EQ(gfx::Point(), action_target->MaximumScrollOffset());
  EXPECT_FALSE(action_target->SetSelection(nullptr, 0, nullptr, 0));
  EXPECT_FALSE(action_target->ScrollToMakeVisible());
  EXPECT_FALSE(action_target->ScrollToMakeVisibleWithSubFocus(
      gfx::Rect(), ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollBehavior::kDoNotScrollIfVisible));
}

}  // namespace ui
