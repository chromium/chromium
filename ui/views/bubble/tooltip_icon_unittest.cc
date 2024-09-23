// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/tooltip_icon.h"

#include <memory>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

namespace views {

using TooltipIconTest = views::ViewsTestBase;

TEST_F(TooltipIconTest, AccessibleRoleAndName) {
  std::u16string tooltip_text = u"Tooltip text";
  std::unique_ptr<views::TooltipIcon> tooltip =
      std::make_unique<views::TooltipIcon>(tooltip_text, 12);

  IgnoreMissingWidgetForTestingScopedSetter a11y_ignore_missing_widget_(
      tooltip->GetViewAccessibility());

  ui::AXNodeData data;
  tooltip->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kStaticText);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            tooltip_text);
  EXPECT_EQ(tooltip->GetViewAccessibility().GetCachedName(), tooltip_text);
}

}  // namespace views
