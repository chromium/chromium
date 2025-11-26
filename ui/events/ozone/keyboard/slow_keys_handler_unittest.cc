// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/keyboard/slow_keys_handler.h"

#include <linux/input-event-codes.h>

#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"

namespace ui {

using testing::ElementsAre;

namespace {

class SlowKeysHandlerTest : public testing::Test {
 protected:
  static constexpr base::TimeDelta kTestSlowKeysDelay =
      base::Milliseconds(1000);
  static constexpr int kTestDeviceId = 0;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    handler_.SetEnabled(true);
    handler_.SetDelay(kTestSlowKeysDelay);
  }

  bool UpdateKeyStateAndShouldDispatch(unsigned int key,
                                       bool down,
                                       int device_id,
                                       base::TimeTicks timestamp) {
    return handler_.UpdateKeyStateAndShouldDispatch(
        key, down, timestamp, device_id,
        MakeOnKeyChangeCallback(key, down, device_id));
  }

  base::OnceCallback<void(base::TimeTicks)>
  MakeOnKeyChangeCallback(unsigned int key, bool down, int device_id) {
    return base::BindLambdaForTesting([=, this](base::TimeTicks timestamp) {
      this->on_key_change_callback_call_args_.push_back(timestamp);
      this->should_dispatch_return_values_.push_back(
          UpdateKeyStateAndShouldDispatch(key, down, device_id, timestamp));
    });
  }

  void FastForwardByDelayMultiplier(float multiplier) {
    task_environment_.FastForwardBy(kTestSlowKeysDelay * multiplier);
  }
  void FastForwardWithinDelay() { FastForwardByDelayMultiplier(0.1); }
  void FastForwardPastDelay() { FastForwardByDelayMultiplier(1.5); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SlowKeysHandler handler_;
  std::vector<base::TimeTicks> on_key_change_callback_call_args_;
  std::vector<bool> should_dispatch_return_values_;
};

}  // namespace

TEST_F(SlowKeysHandlerTest, SetEnabled) {
  handler_.SetEnabled(true);
  EXPECT_TRUE(handler_.IsEnabled());

  handler_.SetEnabled(false);
  EXPECT_FALSE(handler_.IsEnabled());
}

TEST_F(SlowKeysHandlerTest, SetDelay) {
  const base::TimeDelta delay = base::Milliseconds(456);
  handler_.SetDelay(delay);
  EXPECT_EQ(handler_.GetDelay(), delay);
}

TEST_F(SlowKeysHandlerTest, SetNegativeDelay) {
  ASSERT_EQ(handler_.GetDelay(), kTestSlowKeysDelay);
  handler_.SetDelay(base::Milliseconds(-345));
  EXPECT_EQ(handler_.GetDelay(), kTestSlowKeysDelay);
}

TEST_F(SlowKeysHandlerTest, SetLargeDelay) {
  ASSERT_EQ(handler_.GetDelay(), kTestSlowKeysDelay);
  handler_.SetDelay(base::Minutes(10));
  EXPECT_EQ(handler_.GetDelay(), kTestSlowKeysDelay);
}

TEST_F(SlowKeysHandlerTest, AlwaysDispatchWhenDisabled) {
  handler_.SetEnabled(false);
  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);
}

TEST_F(SlowKeysHandlerTest, KeyReleasedBeforeDelayDiscarded) {
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardWithinDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/false, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);
}

TEST_F(SlowKeysHandlerTest, KeyHeldPastDelayAccepted) {
  const auto original_timestamp = EventTimeForNow();

  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, original_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardPastDelay();
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(original_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true));

  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/false, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 1u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 1u);
}

TEST_F(SlowKeysHandlerTest, ModifierKey) {
  const auto original_timestamp = EventTimeForNow();

  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_LEFTCTRL, /*down=*/true, kTestDeviceId, original_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardPastDelay();
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(original_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true));

  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(
      KEY_LEFTCTRL, /*down=*/false, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 1u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 1u);
}

TEST_F(SlowKeysHandlerTest, DifferentKeysHaveIndependentDelays) {
  // t = 0.0, KEY_A delay ends at 1.0
  const auto key_a_down_timestamp = EventTimeForNow();
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, key_a_down_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  // t = 0.7, KEY_B delay ends at 1.7
  FastForwardByDelayMultiplier(0.7);
  const auto key_b_down_timestamp = EventTimeForNow();
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_B, /*down=*/true, kTestDeviceId, key_b_down_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  // t = 1.3
  FastForwardByDelayMultiplier(0.6);
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(key_a_down_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true));

  // t = 1.8
  FastForwardByDelayMultiplier(0.5);
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(key_a_down_timestamp + kTestSlowKeysDelay,
                          key_b_down_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true, true));

  // Verify key ups get dispatched without any additional key change calls.
  FastForwardPastDelay();
  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/false, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 2u);

  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(
      KEY_B, /*down=*/false, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 2u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 2u);
}

TEST_F(SlowKeysHandlerTest, DifferentDevicesHaveIndependentDelays) {
  const int device_id_1 = 1;
  const int device_id_2 = 2;

  // t = 0.0, device 1 delay ends at 1.0
  const auto device_1_down_timestamp = EventTimeForNow();
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, device_id_1, device_1_down_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  // t = 0.7, device 2 delay ends at 1.7
  FastForwardByDelayMultiplier(0.7);
  const auto device_2_down_timestamp = EventTimeForNow();
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, device_id_2, device_2_down_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  // t = 1.3
  FastForwardByDelayMultiplier(0.6);
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(device_1_down_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true));

  // t = 1.8
  FastForwardByDelayMultiplier(0.5);
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(device_1_down_timestamp + kTestSlowKeysDelay,
                          device_2_down_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true, true));

  // Verify key ups get dispatched without any additional key change calls.
  FastForwardPastDelay();
  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(KEY_A, /*down=*/false,
                                              device_id_1, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 2u);

  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(KEY_A, /*down=*/false,
                                              device_id_2, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 2u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 2u);
}

TEST_F(SlowKeysHandlerTest, ExtraKeyPressesBeforeDelayDiscarded) {
  const auto original_timestamp = EventTimeForNow();

  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, original_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  // Extra key press.
  FastForwardWithinDelay();
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardPastDelay();
  EXPECT_THAT(on_key_change_callback_call_args_,
              ElementsAre(original_timestamp + kTestSlowKeysDelay));
  EXPECT_THAT(should_dispatch_return_values_, ElementsAre(true));

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 1u);
}

// This can happen when slow keys is enabled while a key is held down then
// released afterward.
TEST_F(SlowKeysHandlerTest, KeyUpWithoutKeyDown) {
  const auto original_timestamp = EventTimeForNow();

  EXPECT_TRUE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/false, kTestDeviceId, original_timestamp));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);
}

TEST_F(SlowKeysHandlerTest, DelayedKeysClearedWhenDisabling) {
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  handler_.SetEnabled(false);

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);
}

TEST_F(SlowKeysHandlerTest, DelayedKeysClearedWhenChangingDelay) {
  EXPECT_FALSE(UpdateKeyStateAndShouldDispatch(
      KEY_A, /*down=*/true, kTestDeviceId, EventTimeForNow()));
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);

  handler_.SetDelay(handler_.GetDelay() + base::Milliseconds(10));

  FastForwardPastDelay();
  ASSERT_EQ(on_key_change_callback_call_args_.size(), 0u);
}

}  // namespace ui
