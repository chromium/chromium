// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_media_keys_listener_win.h"
#include "ui/events/event.h"

namespace ui {

namespace {

class MockMediaKeysListenerDelegate : public MediaKeysListener::Delegate {
 public:
  MockMediaKeysListenerDelegate() = default;

  MockMediaKeysListenerDelegate(const MockMediaKeysListenerDelegate&) = delete;
  MockMediaKeysListenerDelegate& operator=(
      const MockMediaKeysListenerDelegate&) = delete;

  ~MockMediaKeysListenerDelegate() override = default;

  // MediaKeysListener::Delegate implementation.
  void OnMediaKeysAccelerator(const Accelerator& accelerator) override {
    received_events_.push_back(accelerator.ToKeyEvent());

    // If we've received the events we're waiting for, stop waiting.
    if (key_event_wait_loop_ &&
        received_events_.size() >= num_key_events_to_wait_for_) {
      key_event_wait_loop_->Quit();
    }
  }

  // Loop until we've received |num_events| key events from the listener.
  void WaitForKeyEvents(uint32_t num_events) {
    key_event_wait_loop_ = std::make_unique<base::RunLoop>();
    if (received_events_.size() >= num_events)
      return;

    num_key_events_to_wait_for_ = num_events;
    key_event_wait_loop_->Run();
  }

  // Expect that we have received the correct number of key events.
  void ExpectReceivedEventsCount(uint32_t count) {
    EXPECT_EQ(count, received_events_.size());
  }

  // Expect that the key event received at |index| has the specified key code.
  void ExpectReceivedEvent(uint32_t index, KeyboardCode code) {
    ASSERT_LT(index, received_events_.size());
    KeyEvent* key_event = &received_events_.at(index);
    EXPECT_EQ(code, key_event->key_code());
    EXPECT_EQ(EventType::kKeyPressed, key_event->type());
  }

 private:
  std::vector<KeyEvent> received_events_;
  std::unique_ptr<base::RunLoop> key_event_wait_loop_;
  uint32_t num_key_events_to_wait_for_ = 0;
};

}  // anonymous namespace

class GlobalMediaKeysListenerWinInteractiveTest : public testing::Test {
 public:
  GlobalMediaKeysListenerWinInteractiveTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  GlobalMediaKeysListenerWinInteractiveTest(
      const GlobalMediaKeysListenerWinInteractiveTest&) = delete;
  GlobalMediaKeysListenerWinInteractiveTest& operator=(
      const GlobalMediaKeysListenerWinInteractiveTest&) = delete;

 protected:
  void SendKeyDown(KeyboardCode code) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.time = time_stamp_++;
    input.ki.dwFlags = 0;
    ::SendInput(1, &input, sizeof(INPUT));
  }

  void SendKeyUp(KeyboardCode code) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.time = time_stamp_++;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(1, &input, sizeof(INPUT));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  DWORD time_stamp_ = 0;
};

TEST_F(GlobalMediaKeysListenerWinInteractiveTest, SimplePlayPauseTest) {
  MockMediaKeysListenerDelegate delegate;
  GlobalMediaKeysListenerWin listener(&delegate);

  listener.StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);

  // Send a key press and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_PLAY_PAUSE);
  SendKeyUp(ui::VKEY_MEDIA_PLAY_PAUSE);
  delegate.WaitForKeyEvents(1);
  delegate.ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_PLAY_PAUSE);
}

TEST_F(GlobalMediaKeysListenerWinInteractiveTest, KeyCanBeReRegistered) {
  MockMediaKeysListenerDelegate delegate;
  GlobalMediaKeysListenerWin listener(&delegate);

  // Start listening to register the key.
  listener.StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Stop listening to unregister the key.
  listener.StopWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Start listening to re-register the key.
  listener.StartWatchingMediaKey(ui::VKEY_MEDIA_NEXT_TRACK);

  // Send a key press and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_NEXT_TRACK);
  SendKeyUp(ui::VKEY_MEDIA_NEXT_TRACK);
  delegate.WaitForKeyEvents(1);
  delegate.ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_NEXT_TRACK);
}

TEST_F(GlobalMediaKeysListenerWinInteractiveTest, ListenForMultipleKeys) {
  MockMediaKeysListenerDelegate delegate;
  GlobalMediaKeysListenerWin listener(&delegate);

  listener.StartWatchingMediaKey(ui::VKEY_MEDIA_PLAY_PAUSE);
  listener.StartWatchingMediaKey(ui::VKEY_MEDIA_STOP);

  // Send a key press and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_PLAY_PAUSE);
  SendKeyUp(ui::VKEY_MEDIA_PLAY_PAUSE);
  delegate.WaitForKeyEvents(1);
  delegate.ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_PLAY_PAUSE);

  // Send a key press and validate that it was received by the delegate.
  SendKeyDown(ui::VKEY_MEDIA_STOP);
  SendKeyUp(ui::VKEY_MEDIA_STOP);
  delegate.WaitForKeyEvents(2);
  delegate.ExpectReceivedEvent(/*index=*/1, ui::VKEY_MEDIA_STOP);
}

}  // namespace ui
