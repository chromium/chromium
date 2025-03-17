// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/get_elevation_icon.h"

#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx::win {

TEST(GetElevationIconTest, Do) {
  base::win::ScopedCOMInitializer com(base::win::ScopedCOMInitializer::kMTA);
  ASSERT_TRUE(com.Succeeded());
  ASSERT_EQ(base::win::UserAccountControlIsEnabled(),
            !GetElevationIcon().empty());
}

}  // namespace gfx::win
