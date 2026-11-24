// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_snapshot.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace display {

namespace {

std::unique_ptr<DisplaySnapshot> CreateSnapshotWithEdidDisplayId(
    int64_t port_display_id,
    int64_t edid_display_id) {
  return std::make_unique<DisplaySnapshot>(
      port_display_id, port_display_id, edid_display_id,
      /*connector_index=*/0x0001, gfx::Point{}, gfx::Size{},
      DISPLAY_CONNECTION_TYPE_UNKNOWN, /*base_connector_id=*/1u,
      /*path_topology=*/std::vector<uint64_t>(), false, false,
      PrivacyScreenState::kNotSupported, false, DisplaySnapshot::ColorInfo(),
      std::string(), base::FilePath(), DisplaySnapshot::DisplayModeList(),
      PanelOrientation::kNormal, std::vector<uint8_t>(), nullptr, nullptr, 0, 0,
      gfx::Size{}, VariableRefreshRateState::kVrrNotCapable,
      DrmFormatsAndModifiers());
}

}  // namespace

TEST(DisplaySnapshotTest, ClearCollidingEdidDisplayIds) {
  constexpr int64_t kEdidIdA = kEdidBasedDisplayIdFlag | 0xA;
  constexpr int64_t kEdidIdB = kEdidBasedDisplayIdFlag | 0xB;

  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots;
  snapshots.push_back(CreateSnapshotWithEdidDisplayId(0x101, kEdidIdA));
  snapshots.push_back(CreateSnapshotWithEdidDisplayId(0x102, kEdidIdA));
  snapshots.push_back(CreateSnapshotWithEdidDisplayId(0x103, kEdidIdB));
  snapshots.push_back(
      CreateSnapshotWithEdidDisplayId(0x104, kInvalidDisplayId));
  snapshots.push_back(CreateSnapshotWithEdidDisplayId(0x105, kEdidIdA));

  EXPECT_TRUE(DisplaySnapshot::ClearCollidingEdidDisplayIds(snapshots));

  EXPECT_EQ(snapshots[0]->edid_display_id(), kInvalidDisplayId);
  EXPECT_EQ(snapshots[1]->edid_display_id(), kInvalidDisplayId);
  EXPECT_EQ(snapshots[2]->edid_display_id(), kEdidIdB);
  EXPECT_EQ(snapshots[3]->edid_display_id(), kInvalidDisplayId);
  EXPECT_EQ(snapshots[4]->edid_display_id(), kInvalidDisplayId);

  // Calling again detects no collision and leaves unique IDs intact.
  EXPECT_FALSE(DisplaySnapshot::ClearCollidingEdidDisplayIds(snapshots));
  EXPECT_EQ(snapshots[2]->edid_display_id(), kEdidIdB);
}

}  // namespace display
