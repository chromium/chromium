// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace ui {

class MediaKeyboardHookWinInteractiveTest : public testing::Test {
 public:
  MediaKeyboardHookWinInteractiveTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  void SetUp() override {
    keyboard_hook_ = KeyboardHook::CreateMediaKeyboardHook(base::BindRepeating(
        &MediaKeyboardHookWinInteractiveTest::HandleKeyEvent,
        base::Unretained(this)));
    ASSERT_NE(nullptr, keyboard_hook_);
  }

  // Loop until we've received |num_events| key events from the hook.
  void WaitForKeyEvents(uint32_t num_events) {
    if (key_events_.size() >= num_events)
      return;

    num_key_events_to_wait_for_ = num_events;
    key_event_wait_loop_.Run();
  }

  void SendKeyDown(KeyboardCode code) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.time = time_stamp_++;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));
  }

  void SendKeyUp(KeyboardCode code) {
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = code;
    input.ki.time = time_stamp_++;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
  }

  // Expect that we have received the correct number of key events.
  void ExpectReceivedEventsCount(uint32_t count) {
    EXPECT_EQ(count, key_events_.size());
  }

  // Expect that the key event received at |index| has the specified key code
  // and type.
  void ExpectReceivedEvent(uint32_t index, KeyboardCode code, EventType type) {
    ASSERT_LT(index, key_events_.size());
    KeyEvent* key_event = &key_events_.at(index);
    EXPECT_EQ(code, key_event->key_code());
    EXPECT_EQ(type, key_event->type());
  }

 private:
  void HandleKeyEvent(KeyEvent* key_event) {
    key_events_.push_back(*key_event);
    key_event->SetHandled();

    // If we've received the events we're waiting for, stop waiting.
    if (key_event_wait_loop_.running() &&
        key_events_.size() >= num_key_events_to_wait_for_) {
      key_event_wait_loop_.Quit();
    }
  }

  std::vector<KeyEvent> key_events_;
  std::unique_ptr<KeyboardHook> keyboard_hook_;
  base::test::TaskEnvironment task_environment_;
  base::RunLoop key_event_wait_loop_;
  uint32_t num_key_events_to_wait_for_ = 0;
  DWORD time_stamp_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MediaKeyboardHookWinInteractiveTest);
};

// Test that we catch the different media key events.
TEST_F(MediaKeyboardHookWinInteractiveTest, AllMediaKeysAreCaught) {
  SendKeyDown(ui::VKEY_MEDIA_PLAY_PAUSE);
  SendKeyUp(ui::VKEY_MEDIA_PLAY_PAUSE);
  SendKeyDown(ui::VKEY_MEDIA_STOP);
  SendKeyUp(ui::VKEY_MEDIA_STOP);
  SendKeyDown(ui::VKEY_MEDIA_NEXT_TRACK);
  SendKeyUp(ui::VKEY_MEDIA_NEXT_TRACK);
  SendKeyDown(ui::VKEY_MEDIA_PREV_TRACK);
  SendKeyUp(ui::VKEY_MEDIA_PREV_TRACK);

  // We should receive 8 different key events.
  WaitForKeyEvents(8);
}

// Test that the received events have the proper state.
TEST_F(MediaKeyboardHookWinInteractiveTest, CallbackReceivesProperEvents) {
  // Send a key down event and validate it when received through the hook.
  SendKeyDown(ui::VKEY_MEDIA_PLAY_PAUSE);
  WaitForKeyEvents(1);
  ExpectReceivedEvent(/*index=*/0, ui::VKEY_MEDIA_PLAY_PAUSE, ET_KEY_PRESSED);

  // Send a key up event and validate it when received through the hook.
  SendKeyUp(ui::VKEY_MEDIA_PLAY_PAUSE);
  WaitForKeyEvents(2);
  ExpectReceivedEvent(/*index=*/1, ui::VKEY_MEDIA_PLAY_PAUSE, ET_KEY_RELEASED);
}

}  // namespace ui
