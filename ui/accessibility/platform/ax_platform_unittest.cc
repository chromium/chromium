// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform.h"

#include "base/scoped_observation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform_node.h"

namespace ui {

using ::testing::_;

namespace {

class MockAXModeObserver : public AXModeObserver {
 public:
  MOCK_METHOD(void, OnAXModeAdded, (AXMode mode), (override));
};

}  // namespace

// The test harness creates an instance. Make sure the getter works.
TEST(AXPlatformTest, GetInstance) {
  [[maybe_unused]] auto& instance = AXPlatform::GetInstance();
}

// Tests that observers are notified when mode flags are added.
TEST(AXPlatformTest, Observer) {
  auto& ax_platform = AXPlatform::GetInstance();

  ::testing::StrictMock<MockAXModeObserver> mock_observer;

  base::ScopedObservation<AXPlatform, AXModeObserver> scoped_observation(
      &mock_observer);
  scoped_observation.Observe(&ax_platform);

  EXPECT_CALL(mock_observer, OnAXModeAdded(kAXModeBasic));
  ax_platform.SetMode(kAXModeBasic);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // A second call should be a no-op.
  ax_platform.SetMode(kAXModeBasic);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Removing mode flags should not notify.
  ax_platform.SetMode(AXMode::kNone);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

}  // namespace ui
