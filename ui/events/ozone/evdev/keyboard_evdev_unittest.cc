// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/keyboard_evdev.h"

#include <linux/input-event-codes.h>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

namespace ui {

class KeyboardEvdevTest : public testing::Test {
 public:
  KeyboardEvdevTest() = default;
  KeyboardEvdevTest(const KeyboardEvdevTest&) = delete;
  KeyboardEvdevTest& operator=(const KeyboardEvdevTest&) = delete;
  ~KeyboardEvdevTest() override = default;

 protected:
  static constexpr base::TimeDelta kTestAutoRepeatDelay =
      base::Milliseconds(300);
  static constexpr base::TimeDelta kTestAutoRepeatInterval =
      base::Milliseconds(100);
  static constexpr int kTestDeviceId = 0;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    kb_evdev_ = std::make_unique<KeyboardEvdev>(
        &modifiers_, &stub_kb_layout_engine_,
        base::BindRepeating(&KeyboardEvdevTest::OnEventDispatch,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&KeyboardEvdevTest::OnAnyKeysPressed,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void OnEventDispatch(Event* event) {
    dispatched_events_.push_back(event->Clone());
  }

  void OnAnyKeysPressed(bool any) { any_keys_pressed_.push_back(any); }

  unsigned int MakeFakeScanCode(unsigned int key) { return key + 1000u; }

  void PressKey(unsigned int key, base::TimeTicks timestamp) {
    kb_evdev_->OnKeyChange(key, MakeFakeScanCode(key), /*down=*/true,
                           /*suppress_auto_repeat=*/false, timestamp,
                           kTestDeviceId, EF_NONE);
  }

  void ReleaseKey(unsigned int key, base::TimeTicks timestamp) {
    kb_evdev_->OnKeyChange(key, MakeFakeScanCode(key), /*down=*/false,
                           /*suppress_auto_repeat=*/false, timestamp,
                           kTestDeviceId, EF_NONE);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ui::EventModifiers modifiers_;
  ui::StubKeyboardLayoutEngine stub_kb_layout_engine_;
  std::unique_ptr<KeyboardEvdev> kb_evdev_;

  std::vector<std::unique_ptr<Event>> dispatched_events_;
  std::vector<bool> any_keys_pressed_;

  base::WeakPtrFactory<KeyboardEvdevTest> weak_ptr_factory_{this};
};

TEST_F(KeyboardEvdevTest, HeldKeyAutoRepeats) {
  kb_evdev_->SetAutoRepeatRate(kTestAutoRepeatDelay, kTestAutoRepeatInterval);
  const auto timestamp = EventTimeForNow();

  PressKey(KEY_A, timestamp);
  ASSERT_EQ(dispatched_events_.size(), 1u);
  KeyEvent* key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp);
  EXPECT_FALSE(key_event->is_repeat());

  task_environment_.FastForwardBy(kTestAutoRepeatDelay +
                                  (kTestAutoRepeatInterval * 2.5));
  ASSERT_EQ(dispatched_events_.size(), 4u);

  key_event = dispatched_events_.at(1)->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp + kTestAutoRepeatDelay);
  EXPECT_TRUE(key_event->is_repeat());

  key_event = dispatched_events_.at(2)->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(),
            timestamp + kTestAutoRepeatDelay + kTestAutoRepeatInterval);
  EXPECT_TRUE(key_event->is_repeat());

  key_event = dispatched_events_.at(3)->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(),
            timestamp + kTestAutoRepeatDelay + (kTestAutoRepeatInterval * 2));
  EXPECT_TRUE(key_event->is_repeat());

  ReleaseKey(KEY_A, EventTimeForNow());
  ASSERT_EQ(dispatched_events_.size(), 5u);
  key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyReleased);
  EXPECT_EQ(key_event->time_stamp(), EventTimeForNow());
  EXPECT_FALSE(key_event->is_repeat());

  task_environment_.FastForwardBy(kTestAutoRepeatDelay * 2);
  ASSERT_EQ(dispatched_events_.size(), 5u);
}

class KeyboardEvdevSlowKeysTest : public KeyboardEvdevTest {
 public:
  KeyboardEvdevSlowKeysTest() = default;
  KeyboardEvdevSlowKeysTest(const KeyboardEvdevSlowKeysTest&) = delete;
  KeyboardEvdevSlowKeysTest& operator=(const KeyboardEvdevSlowKeysTest&) =
      delete;
  ~KeyboardEvdevSlowKeysTest() override = default;

 protected:
  static constexpr base::TimeDelta kTestSlowKeysDelay =
      base::Milliseconds(1000);

  // KeyboardEvdevTest:
  void SetUp() override {
    KeyboardEvdevTest::SetUp();

    ASSERT_FALSE(kb_evdev_->IsSlowKeysEnabled());
    kb_evdev_->SetSlowKeysEnabled(true);
    kb_evdev_->SetSlowKeysDelay(kTestSlowKeysDelay);
    ASSERT_TRUE(kb_evdev_->IsSlowKeysEnabled());

    kb_evdev_->SetAutoRepeatEnabled(false);
  }

  void FastForwardWithinSlowKeysDelay() {
    task_environment_.FastForwardBy(kTestSlowKeysDelay * 0.2);
  }
  void FastForwardPastSlowKeysDelay() {
    task_environment_.FastForwardBy(kTestSlowKeysDelay * 1.5);
  }
};

TEST_F(KeyboardEvdevSlowKeysTest, SlowKeysDisabled) {
  kb_evdev_->SetSlowKeysEnabled(false);
  EXPECT_FALSE(kb_evdev_->IsSlowKeysEnabled());

  const base::TimeTicks timestamp = EventTimeForNow();

  PressKey(KEY_A, timestamp);
  ASSERT_EQ(dispatched_events_.size(), 1u);
  KeyEvent* key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp);
}

TEST_F(KeyboardEvdevSlowKeysTest, SuppressAutoRepeatBypassesSlowKeys) {
  const base::TimeTicks timestamp = EventTimeForNow();

  kb_evdev_->OnKeyChange(KEY_A, KEY_A, /*down=*/true,
                         /*suppress_auto_repeat=*/true, timestamp,
                         kTestDeviceId, EF_NONE);

  ASSERT_EQ(dispatched_events_.size(), 1u);
  KeyEvent* key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp);
}

TEST_F(KeyboardEvdevSlowKeysTest, KeyPressIsDelayed) {
  const base::TimeTicks timestamp = EventTimeForNow();

  PressKey(KEY_A, timestamp);
  ASSERT_EQ(dispatched_events_.size(), 0u);

  FastForwardPastSlowKeysDelay();
  ASSERT_EQ(dispatched_events_.size(), 1u);
  KeyEvent* key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp + kTestSlowKeysDelay);

  ReleaseKey(KEY_A, EventTimeForNow());
  ASSERT_EQ(dispatched_events_.size(), 2u);
  key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyReleased);
  EXPECT_EQ(key_event->time_stamp(), EventTimeForNow());
}

TEST_F(KeyboardEvdevSlowKeysTest, KeyReleaseCancelsDelayedKeyPress) {
  PressKey(KEY_A, EventTimeForNow());
  ASSERT_EQ(dispatched_events_.size(), 0u);

  FastForwardWithinSlowKeysDelay();
  ASSERT_EQ(dispatched_events_.size(), 0u);

  ReleaseKey(KEY_A, EventTimeForNow());
  ASSERT_EQ(dispatched_events_.size(), 0u);

  FastForwardPastSlowKeysDelay();
  ASSERT_EQ(dispatched_events_.size(), 0u);
}

TEST_F(KeyboardEvdevSlowKeysTest, RepeatKeysStartAfterSlowKeys) {
  kb_evdev_->SetAutoRepeatEnabled(true);
  kb_evdev_->SetAutoRepeatRate(kTestAutoRepeatDelay, kTestAutoRepeatInterval);

  const base::TimeTicks timestamp = EventTimeForNow();

  PressKey(KEY_A, timestamp);
  ASSERT_EQ(dispatched_events_.size(), 0u);

  task_environment_.FastForwardBy(kTestSlowKeysDelay);
  ASSERT_EQ(dispatched_events_.size(), 1u);
  KeyEvent* key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp + kTestSlowKeysDelay);

  task_environment_.FastForwardBy(kTestAutoRepeatDelay);
  ASSERT_EQ(dispatched_events_.size(), 2u);
  key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(),
            timestamp + kTestSlowKeysDelay + kTestAutoRepeatDelay);

  task_environment_.FastForwardBy(kTestAutoRepeatInterval);
  ASSERT_EQ(dispatched_events_.size(), 3u);
  key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyPressed);
  EXPECT_EQ(key_event->time_stamp(), timestamp + kTestSlowKeysDelay +
                                         kTestAutoRepeatDelay +
                                         kTestAutoRepeatInterval);

  ReleaseKey(KEY_A, EventTimeForNow());
  ASSERT_EQ(dispatched_events_.size(), 4u);
  key_event = dispatched_events_.back()->AsKeyEvent();
  EXPECT_EQ(key_event->code(), DomCode::US_A);
  EXPECT_EQ(key_event->type(), EventType::kKeyReleased);
  EXPECT_EQ(key_event->time_stamp(), EventTimeForNow());

  task_environment_.FastForwardBy(kTestSlowKeysDelay + kTestAutoRepeatDelay);
  ASSERT_EQ(dispatched_events_.size(), 4u);
}

}  // namespace ui
