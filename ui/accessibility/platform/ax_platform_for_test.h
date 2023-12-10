// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_FOR_TEST_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_FOR_TEST_H_

#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace ui {

// A process-wide AXPlatform instance for use by tests.
class AXPlatformForTest : public AXPlatform::Delegate {
 public:
  AXPlatformForTest() = default;
  AXPlatformForTest(const AXPlatformForTest&) = delete;
  AXPlatformForTest& operator=(const AXPlatformForTest&) = delete;
  ~AXPlatformForTest() override = default;

  // AXPlatform::Delegate:
  AXMode GetProcessMode() override;
  void SetProcessMode(AXMode new_mode) override;

 private:
  AXPlatform ax_platform_{*this};

  AXMode mode_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_FOR_TEST_H_
