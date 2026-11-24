// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_id.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace display::internal {

namespace {

constexpr int64_t kPortId = 2200000257;
constexpr int64_t kEdidDisplayId = kEdidBasedDisplayIdFlag | 0x123456789;

}  // namespace

TEST(DisplayIdTest, RuntimeId) {
  const DisplayId port_only{.port_id = kPortId};
  EXPECT_FALSE(port_only.has_edid_display_id());
  EXPECT_EQ(port_only.runtime_id(), kPortId);

  const DisplayId with_edid_display_id{.port_id = kPortId,
                                       .edid_display_id = kEdidDisplayId};
  EXPECT_TRUE(with_edid_display_id.has_edid_display_id());
  EXPECT_EQ(with_edid_display_id.runtime_id(), kEdidDisplayId);

  EXPECT_EQ(DisplayId{}.runtime_id(), kInvalidDisplayId);
}

TEST(DisplayIdTest, OtherId) {
  EXPECT_EQ((DisplayId{.port_id = kPortId}.other_id()), kInvalidDisplayId);

  const DisplayId uses_edid{.port_id = kPortId,
                            .edid_display_id = kEdidDisplayId};
  EXPECT_EQ(uses_edid.other_id(), kPortId);

  // The EDID-based display ID is known but not used (e.g. EDID-based display
  // IDs are disabled).
  const DisplayId unused_edid_display_id{.port_id = kPortId,
                                         .edid_display_id = kEdidDisplayId,
                                         .uses_edid_display_id = false};
  EXPECT_TRUE(unused_edid_display_id.has_edid_display_id());
  EXPECT_EQ(unused_edid_display_id.runtime_id(), kPortId);
  EXPECT_EQ(unused_edid_display_id.other_id(), kEdidDisplayId);
}

TEST(DisplayIdTest, Print) {
  std::ostringstream stream;
  stream << DisplayId{.port_id = 1, .edid_display_id = 2};
  EXPECT_EQ(stream.str(),
            "{port_id=1, edid_display_id=2, uses_edid_display_id=1}");
}

}  // namespace display::internal
