// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_PROVIDE_AX_PLATFORM_FOR_TESTS_H_
#define UI_ACCESSIBILITY_PLATFORM_PROVIDE_AX_PLATFORM_FOR_TESTS_H_

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"

namespace ui {

// A test event listener that provides an `AXPlatformForTest`. An instance is
// guaranteed to be alive for the lifetime of the listener, and is reset
// following each test.
class ProvideAXPlatformForTests : public ::testing::EmptyTestEventListener {
 public:
  ProvideAXPlatformForTests();
  ProvideAXPlatformForTests(const ProvideAXPlatformForTests&) = delete;
  ProvideAXPlatformForTests& operator=(const ProvideAXPlatformForTests&) =
      delete;
  ~ProvideAXPlatformForTests() override;

  void OnTestEnd(const ::testing::TestInfo& test_info) override;

 private:
  std::optional<AXPlatformForTest> ax_platform_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_PROVIDE_AX_PLATFORM_FOR_TESTS_H_
