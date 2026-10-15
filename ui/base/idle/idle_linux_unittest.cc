// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/scoped_set_idle_state.h"

namespace ui {

// When the idle state is overridden for testing, registering a screen lock
// callback must not create the shared D-Bus thread and buses, so that tests
// never probe the host's session bus for a ScreenSaver service.
TEST(IdleLinuxTest, AddScreenLockCallbackWithOverrideDoesNotUseDBus) {
  base::test::TaskEnvironment task_environment;
  ASSERT_FALSE(dbus_thread_linux::IsInitializedForTesting())
      << "Another test leaked the shared D-Bus thread.";
  ScopedSetIdleState scoped_idle_state(IDLE_STATE_ACTIVE);

  base::CallbackListSubscription subscription =
      AddScreenLockCallback(base::DoNothing());

  EXPECT_FALSE(dbus_thread_linux::IsInitializedForTesting());

  // If this regresses, shut the D-Bus thread down while the task environment
  // is still alive so that it doesn't leak into later tests.
  if (dbus_thread_linux::IsInitializedForTesting()) {
    dbus_thread_linux::Shutdown();
  }
}

}  // namespace ui
