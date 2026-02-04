// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/screensaver_state_observer.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle.h"
#include "ui/base/idle/scoped_set_idle_state.h"
#include "ui/base/idle/scoped_set_screensaver_state.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace ui {
namespace {

class ScreensaverStateObserverTest : public ::testing::Test {
 public:
  ScreensaverStateObserverTest() = default;
  ~ScreensaverStateObserverTest() override = default;

 protected:
  void SetUp() override {
    // Ensure test state is cleared before each test.
    ScreensaverStateForTesting().reset();
  }

  void TearDown() override {
    // Clean up test state after each test.
    ScreensaverStateForTesting().reset();
  }

  // Task environment is required for SingletonHwnd message loop.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(ScreensaverStateObserverTest, GetInstance_ReturnsValidObserver) {
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  ASSERT_NE(observer, nullptr);
}

TEST_F(ScreensaverStateObserverTest,
       ScopedSetScreensaverState_SetsAndRestoresState) {
  // Verify the state is not set initially.
  EXPECT_FALSE(ScreensaverStateForTesting().has_value());

  {
    // Use ScopedSetScreensaverState to set the override.
    ScopedSetScreensaverState scoped_state(true);

    // The override should now be set.
    EXPECT_TRUE(ScreensaverStateForTesting().has_value());
    EXPECT_TRUE(ScreensaverStateForTesting().value());
  }

  // After the scoped state is destroyed, the override should be cleared.
  EXPECT_FALSE(ScreensaverStateForTesting().has_value());
}

TEST_F(ScreensaverStateObserverTest,
       ScopedSetScreensaverState_RestoresPreviousState) {
  {
    ScopedSetScreensaverState outer_state(false);
    EXPECT_TRUE(ScreensaverStateForTesting().has_value());
    EXPECT_FALSE(ScreensaverStateForTesting().value());

    {
      // Nested scoped state should override.
      ScopedSetScreensaverState inner_state(true);
      EXPECT_TRUE(ScreensaverStateForTesting().value());
    }

    // The outer state (false) should be restored.
    EXPECT_TRUE(ScreensaverStateForTesting().has_value());
    EXPECT_FALSE(ScreensaverStateForTesting().value());
  }

  // After all scoped states are destroyed, no override should remain.
  EXPECT_FALSE(ScreensaverStateForTesting().has_value());
}

TEST_F(ScreensaverStateObserverTest,
       CheckIdleStateIsLocked_RespectsScreensaverOverride) {
  // When screensaver is overridden to true, CheckIdleStateIsLocked should
  // return true.
  {
    ScopedSetScreensaverState scoped_state(true);
    EXPECT_TRUE(CheckIdleStateIsLocked());
  }

  // When screensaver is overridden to false and workstation is not locked,
  // CheckIdleStateIsLocked should return false.
  {
    ScopedSetScreensaverState scoped_state(false);
    // Note: This may still return true if IsWorkstationLocked() returns true,
    // but in a typical test environment, the workstation should not be locked.
    EXPECT_FALSE(CheckIdleStateIsLocked());
  }
}

TEST_F(ScreensaverStateObserverTest,
       RefreshScreensaverState_UpdatesCachedValue) {
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  ASSERT_NE(observer, nullptr);

  // Set test override to true.
  {
    ScopedSetScreensaverState scoped_state(true);
    EXPECT_TRUE(observer->IsScreensaverRunning());
  }

  // Set test override to false.
  {
    ScopedSetScreensaverState scoped_state(false);
    EXPECT_FALSE(observer->IsScreensaverRunning());
  }
}

TEST_F(ScreensaverStateObserverTest,
       WndProc_ScreensaverActiveChange_TriggersUpdate) {
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  ASSERT_NE(observer, nullptr);

  // Set initial cached state to false.
  ScopedSetScreensaverState scoped_state(false);
  EXPECT_FALSE(observer->IsScreensaverRunning());

  // Now change the test override to true.
  ScreensaverStateForTesting() = true;

  // Simulate a WM_SETTINGCHANGE message with SPI_SETSCREENSAVEACTIVE.
  // This updates the cached state synchronously.
  gfx::SingletonHwnd* singleton_hwnd = gfx::SingletonHwnd::GetInstance();
  LRESULT result = 0;
  singleton_hwnd->ProcessWindowMessage(singleton_hwnd->hwnd(), WM_SETTINGCHANGE,
                                       SPI_SETSCREENSAVEACTIVE, 0, result, 0);

  // Verify the cached value was updated to the new test override value.
  EXPECT_TRUE(observer->IsScreensaverRunning());
}

TEST_F(ScreensaverStateObserverTest,
       WndProc_ScreensaverRunningChange_TriggersUpdate) {
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  ASSERT_NE(observer, nullptr);

  // Set initial cached state to true.
  ScopedSetScreensaverState scoped_state(true);
  EXPECT_TRUE(observer->IsScreensaverRunning());

  // Now change the test override to false.
  ScreensaverStateForTesting() = false;

  // Simulate a WM_SETTINGCHANGE message with SPI_SETSCREENSAVERRUNNING.
  gfx::SingletonHwnd* singleton_hwnd = gfx::SingletonHwnd::GetInstance();
  LRESULT result = 0;
  singleton_hwnd->ProcessWindowMessage(singleton_hwnd->hwnd(), WM_SETTINGCHANGE,
                                       SPI_SETSCREENSAVERRUNNING, 0, result, 0);

  // Verify the cached value was updated to the new test override value.
  EXPECT_FALSE(observer->IsScreensaverRunning());
}

TEST_F(ScreensaverStateObserverTest,
       WndProc_UnrelatedSettingChange_DoesNotTriggerUpdate) {
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  ASSERT_NE(observer, nullptr);

  // Set initial cached state to false.
  ScopedSetScreensaverState scoped_state(false);
  EXPECT_FALSE(observer->IsScreensaverRunning());

  // Change the test override to true, but DON'T send a screensaver-related
  // message.
  ScreensaverStateForTesting() = true;

  // Simulate a WM_SETTINGCHANGE message with an UNRELATED wparam.
  // This should NOT trigger an update.
  gfx::SingletonHwnd* singleton_hwnd = gfx::SingletonHwnd::GetInstance();
  LRESULT result = 0;
  singleton_hwnd->ProcessWindowMessage(singleton_hwnd->hwnd(), WM_SETTINGCHANGE,
                                       SPI_SETKEYBOARDSPEED, 0, result, 0);

  // The cached value should NOT have been updated.
  EXPECT_FALSE(observer->IsScreensaverRunning());
}

TEST_F(ScreensaverStateObserverTest, WndProc_NonSettingChangeMessage_Ignored) {
  ScreensaverStateObserver* observer = ScreensaverStateObserver::GetInstance();
  ASSERT_NE(observer, nullptr);

  // Set initial cached state to false.
  ScopedSetScreensaverState scoped_state(false);
  EXPECT_FALSE(observer->IsScreensaverRunning());

  // Change the test override to true.
  ScreensaverStateForTesting() = true;

  // Simulate a different message type (not WM_SETTINGCHANGE).
  // This should be completely ignored.
  gfx::SingletonHwnd* singleton_hwnd = gfx::SingletonHwnd::GetInstance();
  LRESULT result = 0;
  singleton_hwnd->ProcessWindowMessage(singleton_hwnd->hwnd(), WM_PAINT, 0, 0,
                                       result, 0);

  // The cached value should NOT have been updated.
  EXPECT_FALSE(observer->IsScreensaverRunning());
}

}  // namespace
}  // namespace ui
