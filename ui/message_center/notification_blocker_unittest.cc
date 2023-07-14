// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_blocker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/lock_screen/fake_lock_screen_controller.h"
#include "ui/message_center/message_center.h"

namespace message_center {

namespace {

class TestNotificationBlocker : public NotificationBlocker {
 public:
  explicit TestNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center) {}

  bool ShouldShowNotificationAsPopup(
      const Notification& notification) const override {
    return true;
  }
};

class TestMessageCenter : public FakeMessageCenter {
 public:
  TestMessageCenter() = default;
  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;
  ~TestMessageCenter() override = default;

  void AddNotificationBlocker(NotificationBlocker* blocker) override {
    add_notification_blocker_called_count_++;
  }

  size_t add_notification_blocker_called_count() {
    return add_notification_blocker_called_count_;
  }

 private:
  size_t add_notification_blocker_called_count_ = 0;
};

}  // namespace

class NotificationBlockerTest : public testing::Test {
 public:
  NotificationBlockerTest() = default;
  NotificationBlockerTest(const NotificationBlockerTest&) = delete;
  NotificationBlockerTest& operator=(const NotificationBlockerTest&) = delete;
  ~NotificationBlockerTest() override = default;

  void SetUp() override {
    MessageCenter::Initialize(std::make_unique<FakeLockScreenController>());
  }

  void TearDown() override { MessageCenter::Shutdown(); }
};

// Tests that a `NotificationBlocker` is not registered with the `MessageCenter`
// upon construction, but is registered after `Init()` has been called.
TEST_F(NotificationBlockerTest, OnlyRegisteredAfterInit) {
  TestMessageCenter message_center;
  size_t initial_count = message_center.add_notification_blocker_called_count();

  // Create a `NotificationBlocker`.
  TestNotificationBlocker blocker(&message_center);

  // `blocker` should not have been registered with the `MessageCenter` yet.
  EXPECT_EQ(initial_count,
            message_center.add_notification_blocker_called_count());

  // Initialize `blocker`.
  blocker.Init();

  // Now `blocker` should have been registered with the `MessageCenter`.
  EXPECT_EQ(initial_count + 1,
            message_center.add_notification_blocker_called_count());
}

using NotificationBlockerDeathTest = NotificationBlockerTest;

// Tests that the program crashes if an attempt is made to initialize a
// `NotificationBlocker` more than once.
TEST_F(NotificationBlockerDeathTest, MaxOneInit) {
  // Create and initialize a `NotificationBlocker`.
  TestNotificationBlocker blocker(MessageCenter::Get());
  blocker.Init();

  // Attempting to initialize `blocker` a second time should cause the program
  // to crash.
  std::string log;
#if CHECK_WILL_STREAM()
  log = "Do not initialize a NotificationBlocker more than once\\.";
#else
  log = ".*";
#endif
  EXPECT_DEATH({ blocker.Init(); }, log);
}

}  // namespace message_center
