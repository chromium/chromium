// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/wayland/wayland_display_util.h"

namespace ui::wayland {

TEST(WaylandDisplayUtilTest, Basic) {
  const int64_t kTestIds[] = {
      std::numeric_limits<int64_t>::min(),
      std::numeric_limits<int64_t>::min() + 1,
      static_cast<int64_t>(std::numeric_limits<int32_t>::min()) - 1,
      std::numeric_limits<int32_t>::min(),
      std::numeric_limits<int32_t>::min() + 1,
      -1,
      0,
      1,
      std::numeric_limits<int32_t>::max() - 1,
      std::numeric_limits<int32_t>::max(),
      static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1,
      std::numeric_limits<int64_t>::max() - 1,
      std::numeric_limits<int64_t>::max()};

  for (int64_t id : kTestIds) {
    auto pair = ToWaylandDisplayIdPair(id);
    EXPECT_EQ(id, FromWaylandDisplayIdPair({pair.high, pair.low}));
  }
}

}  // namespace ui::wayland
