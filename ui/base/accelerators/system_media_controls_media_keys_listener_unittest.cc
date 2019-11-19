// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/system_media_controls_media_keys_listener.h"

#include <memory>

#include "components/system_media_controls/mock_system_media_controls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"

using testing::_;
using testing::Expectation;
using testing::WithArg;

namespace ui {

namespace {

class MockMediaKeysListenerDelegate : public MediaKeysListener::Delegate {
 public:
  MockMediaKeysListenerDelegate() = default;
  ~MockMediaKeysListenerDelegate() override = default;

  // MediaKeysListener::Delegate implementation.
  MOCK_METHOD1(OnMediaKeysAccelerator, void(const Accelerator& accelerator));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaKeysListenerDelegate);
};

}  // anonymous namespace

class SystemMediaControlsMediaKeysListenerTest : public testing::Test {
 public:
  SystemMediaControlsMediaKeysListenerTest() {
    listener_ =
        std::make_unique<SystemMediaControlsMediaKeysListener>(&delegate_);
    listener_->SetSystemMediaControlsForTesting(
        &mock_system_media_controls_service_);
  }

  ~SystemMediaControlsMediaKeysListenerTest() override = default;

 protected:
  system_media_controls::testing::MockSystemMediaControls&
  mock_system_media_controls_service() {
    return mock_system_media_controls_service_;
  }
  MockMediaKeysListenerDelegate& delegate() { return delegate_; }
  SystemMediaControlsMediaKeysListener* listener() { return listener_.get(); }

 private:
  system_media_controls::testing::MockSystemMediaControls
      mock_system_media_controls_service_;
  MockMediaKeysListenerDelegate delegate_;
  std::unique_ptr<SystemMediaControlsMediaKeysListener> listener_;

  DISALLOW_COPY_AND_ASSIGN(SystemMediaControlsMediaKeysListenerTest);
};

TEST_F(SystemMediaControlsMediaKeysListenerTest, ListensToSystemMediaControls) {
  EXPECT_CALL(mock_system_media_controls_service(), AddObserver(listener()));
  listener()->Initialize();
}

TEST_F(SystemMediaControlsMediaKeysListenerTest, SimplePlayPauseTest) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_system_media_controls_service(),
              SetIsPlayPauseEnabled(true));

  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_))
      .WillOnce(WithArg<0>([](const Accelerator& accelerator) {
        EXPECT_EQ(ui::VKEY_MEDIA_PLAY_PAUSE, accelerator.key_code());
      }));

  ASSERT_TRUE(listener()->Initialize());
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);

  // Simulate media key press.
  listener()->OnPlay();
}

TEST_F(SystemMediaControlsMediaKeysListenerTest, KeyCanBeReRegistered) {
  Expectation enable_next =
      EXPECT_CALL(mock_system_media_controls_service(), SetIsNextEnabled(true));
  Expectation disable_next =
      EXPECT_CALL(mock_system_media_controls_service(), SetIsNextEnabled(false))
          .After(enable_next);
  Expectation reenable_next =
      EXPECT_CALL(mock_system_media_controls_service(), SetIsNextEnabled(true))
          .After(disable_next);
  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_))
      .After(reenable_next)
      .WillOnce(WithArg<0>([](const Accelerator& accelerator) {
        EXPECT_EQ(ui::VKEY_MEDIA_NEXT_TRACK, accelerator.key_code());
      }));

  ASSERT_TRUE(listener()->Initialize());

  // Start listening to register the key.
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Stop listening to unregister the key.
  listener()->StopWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Start listening to re-register the key.
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Simulate media key press.
  listener()->OnNext();
}

TEST_F(SystemMediaControlsMediaKeysListenerTest, ListenForMultipleKeys) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_system_media_controls_service(),
              SetIsPlayPauseEnabled(true));
  EXPECT_CALL(mock_system_media_controls_service(), SetIsPreviousEnabled(true));

  // Should receive the key presses.
  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_)).Times(2);

  ASSERT_TRUE(listener()->Initialize());
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PREV_TRACK);

  // Simulate media key press.
  listener()->OnPlay();
  listener()->OnPrevious();
}

TEST_F(SystemMediaControlsMediaKeysListenerTest,
       DoesNotFirePlayPauseOnPauseEventWhenPaused) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_system_media_controls_service(),
              SetIsPlayPauseEnabled(true));

  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_)).Times(0);

  ASSERT_TRUE(listener()->Initialize());
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener()->SetIsMediaPlaying(false);

  // Simulate media key press.
  listener()->OnPause();
}

TEST_F(SystemMediaControlsMediaKeysListenerTest,
       DoesNotFirePlayPauseOnPlayEventWhenPlaying) {
  // Should be set to true when we start listening for the key.
  EXPECT_CALL(mock_system_media_controls_service(),
              SetIsPlayPauseEnabled(true));

  EXPECT_CALL(delegate(), OnMediaKeysAccelerator(_)).Times(0);

  ASSERT_TRUE(listener()->Initialize());
  listener()->StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener()->SetIsMediaPlaying(true);

  // Simulate media key press.
  listener()->OnPlay();
}

}  // namespace ui
