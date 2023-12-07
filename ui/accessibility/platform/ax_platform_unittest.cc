// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

// The test harness creates an instance. Make sure the getter works.
TEST(AXPlatformTest, GetInstance) {
  [[maybe_unused]] auto& instance = AXPlatform::GetInstance();
}

}  // namespace ui
