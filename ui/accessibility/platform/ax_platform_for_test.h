// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_FOR_TEST_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_FOR_TEST_H_

#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace ui {

class ScopedAXModeSetter;

// A process-wide AXPlatform instance for use by tests.
class AXPlatformForTest : public AXPlatform::Delegate {
 public:
  // Returns the instance for the test.
  static AXPlatformForTest& GetInstance();

  AXPlatformForTest();
  AXPlatformForTest(const AXPlatformForTest&) = delete;
  AXPlatformForTest& operator=(const AXPlatformForTest&) = delete;
  ~AXPlatformForTest() override;

  // AXPlatform::Delegate:
  AXMode GetProcessMode() override;
  void SetProcessMode(AXMode new_mode) override;
  void OnAccessibilityApiUsage() override;
#if BUILDFLAG(IS_WIN)
  AXPlatform::ProductStrings GetProductStrings() override;
#endif

 private:
  friend class ScopedAXModeSetter;

  AXPlatform ax_platform_{*this};

  AXMode mode_;
};

// Provides a way for tests to temporarily override the accessibility mode flags
// accessible by the test's fake AXPlatform. Observers are not notified of the
// change.
class ScopedAXModeSetter {
 public:
  explicit ScopedAXModeSetter(AXMode new_mode) {
    AXPlatformForTest::GetInstance().mode_ = new_mode;
  }
  ~ScopedAXModeSetter() {
    AXPlatformForTest::GetInstance().mode_ = previous_mode_;
  }

 private:
  const AXMode previous_mode_{AXPlatformForTest::GetInstance().mode_};
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_FOR_TEST_H_
