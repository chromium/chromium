// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_role_properties.h"

#include "base/containers/enum_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

TEST(AXRolePropertiesTest, TestSupportsHierarchicalLevel) {
  // Test for iterating through all roles and validate if a role supports
  // hierarchical level.
  constexpr base::EnumSet<ax::mojom::Role>
      roles_expected_supports_hierarchical_level = {
          ax::mojom::Role::kComment, ax::mojom::Role::kListItem,
          ax::mojom::Role::kRow, ax::mojom::Role::kTreeItem};

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    ax::mojom::Role role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_hierarchical_level = SupportsHierarchicalLevel(role);

    SCOPED_TRACE(testing::Message() << "ax::mojom::Role=" << ToString(role)
                                    << ", Actual: supportsHierarchicalLevel="
                                    << supports_hierarchical_level
                                    << ", Expected: supportsHierarchicalLevel="
                                    << !supports_hierarchical_level);

    if (roles_expected_supports_hierarchical_level.Has(role)) {
      EXPECT_TRUE(supports_hierarchical_level);
    } else {
      EXPECT_FALSE(supports_hierarchical_level);
    }
  }
}

TEST(AXRolePropertiesTest, TestIsLiveRegion) {
  // Test for iterating through all roles and validate if a role is a live
  // region by default (alert, log, status).
  constexpr base::EnumSet<ax::mojom::Role> roles_expected_is_live_region = {
      ax::mojom::Role::kAlert, ax::mojom::Role::kLog, ax::mojom::Role::kStatus};

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    ax::mojom::Role role = static_cast<ax::mojom::Role>(role_idx);
    bool is_live_region = IsLiveRegion(role);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(role)
                 << ", Actual: isLiveRegion=" << is_live_region
                 << ", Expected: isLiveRegion=" << !is_live_region);

    if (roles_expected_is_live_region.Has(role)) {
      EXPECT_TRUE(is_live_region);
    } else {
      EXPECT_FALSE(is_live_region);
    }
  }
}

TEST(AXRolePropertiesTest, TestSupportsToggle) {
  // Test for iterating through all roles and validate if a role supports
  // toggle.
  constexpr base::EnumSet<ax::mojom::Role> roles_expected_supports_toggle = {
      ax::mojom::Role::kCheckBox, ax::mojom::Role::kMenuItemCheckBox,
      ax::mojom::Role::kSwitch, ax::mojom::Role::kToggleButton};

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    ax::mojom::Role role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_toggle = SupportsToggle(role);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(role)
                 << ", Actual: supportsToggle=" << supports_toggle
                 << ", Expected: supportsToggle=" << !supports_toggle);

    if (roles_expected_supports_toggle.Has(role)) {
      EXPECT_TRUE(supports_toggle);
    } else {
      EXPECT_FALSE(supports_toggle);
    }
  }
}

TEST(AXRolePropertiesTest, TestIsTableWithColumns) {
  // Test for iterating through all roles and validate if a role is
  // considered a table which supports multiple columns.
  base::EnumSet<ax::mojom::Role> roles_expected_is_table_with_columns = {
      ax::mojom::Role::kGrid, ax::mojom::Role::kListGrid,
      ax::mojom::Role::kTable, ax::mojom::Role::kTreeGrid};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  roles_expected_is_table_with_columns.Put(ax::mojom::Role::kLayoutTable);
#endif

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    ax::mojom::Role role = static_cast<ax::mojom::Role>(role_idx);
    bool is_table_with_columns = IsTableLike(role);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(role)
                 << ", Actual: isTableWithColumns=" << is_table_with_columns
                 << ", Expected: isTableWithColumns="
                 << !is_table_with_columns);

    if (roles_expected_is_table_with_columns.Has(role)) {
      EXPECT_TRUE(is_table_with_columns);
    } else {
      EXPECT_FALSE(is_table_with_columns);
    }
  }
}
}  // namespace ui
