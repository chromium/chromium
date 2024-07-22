// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/win/keyboard_hook_win_base.h"

namespace ui {

class MediaKeyboardHookWinTest : public testing::Test {
 public:
  MediaKeyboardHookWinTest();

  MediaKeyboardHookWinTest(const MediaKeyboardHookWinTest&) = delete;
  MediaKeyboardHookWinTest& operator=(const MediaKeyboardHookWinTest&) = delete;

  ~MediaKeyboardHookWinTest() override;

  // testing::Test overrides.
  void SetUp() override;

  void HandleKeyPress(KeyEvent* key_event);

 protected:
  KeyboardHookWinBase* keyboard_hook() { return keyboard_hook_.get(); }

  uint32_t next_time_stamp() { return time_stamp_++; }

  std::vector<KeyEvent>* key_events() { return &key_events_; }

  // Used for sending key events which are handled by the hook.
  void SendMediaKeyDownEvent(KeyboardCode key_code,
                             DomCode dom_code,
                             int repeat_count = 1);
  void SendMediaKeyUpEvent(KeyboardCode key_code, DomCode dom_code);

  // Set the return value for the HandleKeyPress callback.
  void StartHandlingKeys() { should_handle_keys_ = true; }
  void StopHandlingKeys() { should_handle_keys_ = false; }

 private:
  uint32_t time_stamp_ = 0;
  std::unique_ptr<KeyboardHookWinBase> keyboard_hook_;
  std::vector<KeyEvent> key_events_;
  bool should_handle_keys_ = true;
};

MediaKeyboardHookWinTest::MediaKeyboardHookWinTest() = default;

MediaKeyboardHookWinTest::~MediaKeyboardHookWinTest() = default;

void MediaKeyboardHookWinTest::SetUp() {
  keyboard_hook_ = KeyboardHookWinBase::CreateMediaKeyboardHookForTesting(
      base::BindRepeating(&MediaKeyboardHookWinTest::HandleKeyPress,
                          base::Unretained(this)));
}

void MediaKeyboardHookWinTest::HandleKeyPress(KeyEvent* key_event) {
  key_events_.push_back(*key_event);
  if (should_handle_keys_)
    key_event->SetHandled();
}

void MediaKeyboardHookWinTest::SendMediaKeyDownEvent(KeyboardCode key_code,
                                                     DomCode dom_code,
                                                     int repeat_count /*=1*/) {
  ASSERT_GT(repeat_count, 0);
  // This should only be used when we're handling keys.
  ASSERT_TRUE(should_handle_keys_);

  for (int i = 0; i < repeat_count; i++) {
    ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
        WM_KEYDOWN, key_code,
        KeycodeConverter::DomCodeToNativeKeycode(dom_code), next_time_stamp()));
  }
}

void MediaKeyboardHookWinTest::SendMediaKeyUpEvent(KeyboardCode key_code,
                                                   DomCode dom_code) {
  // This should only be used when we're handling keys.
  ASSERT_TRUE(should_handle_keys_);

  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, key_code, KeycodeConverter::DomCodeToNativeKeycode(dom_code),
      next_time_stamp()));
}

namespace {
void VerifyKeyEvent(KeyEvent* key_event,
                    KeyboardCode non_located_key_code,
                    DomCode dom_code,
                    bool key_down,
                    bool is_repeat) {
  if (key_down) {
    ASSERT_EQ(key_event->type(), EventType::kKeyPressed);
    ASSERT_EQ(key_event->is_repeat(), is_repeat);
  } else {
    ASSERT_EQ(key_event->type(), EventType::kKeyReleased);
    ASSERT_FALSE(key_event->is_repeat());
  }
  ASSERT_EQ(key_event->key_code(), non_located_key_code);
  ASSERT_EQ(key_event->code(), dom_code);
}
}  // namespace

TEST_F(MediaKeyboardHookWinTest, SimpleKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_MEDIA_PLAY_PAUSE;
  const DomCode dom_code = DomCode::MEDIA_PLAY_PAUSE;
  SendMediaKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1u);
  SendMediaKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2u);

  KeyEvent down_event = key_events()->at(0);
  ASSERT_NO_FATAL_FAILURE(
      VerifyKeyEvent(&down_event, key_code, dom_code, true, false));

  KeyEvent up_event = key_events()->at(1);
  ASSERT_NO_FATAL_FAILURE(
      VerifyKeyEvent(&up_event, key_code, dom_code, false, false));
}

TEST_F(MediaKeyboardHookWinTest, RepeatingKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_MEDIA_PLAY_PAUSE;
  const DomCode dom_code = DomCode::MEDIA_PLAY_PAUSE;
  SendMediaKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendMediaKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    ASSERT_NO_FATAL_FAILURE(
        VerifyKeyEvent(&event, key_code, dom_code, true, should_repeat));
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  ASSERT_NO_FATAL_FAILURE(
      VerifyKeyEvent(&up_event, key_code, dom_code, false, false));
}

TEST_F(MediaKeyboardHookWinTest, UnhandledKeysArePropagated) {
  StopHandlingKeys();

  // Ensure media keys are propagated to the OS.
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_MEDIA_STOP,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::MEDIA_STOP),
      next_time_stamp()));
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_MEDIA_STOP,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::MEDIA_STOP),
      next_time_stamp()));

  StartHandlingKeys();

  // Ensure media keys are not propagated to the OS.
  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_MEDIA_STOP,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::MEDIA_STOP),
      next_time_stamp()));
  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_MEDIA_STOP,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::MEDIA_STOP),
      next_time_stamp()));
}

TEST_F(MediaKeyboardHookWinTest, NonInterceptedKeysTest) {
  // Here we try a few keys we do not expect to be intercepted / handled.
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_RSHIFT,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::SHIFT_RIGHT),
      next_time_stamp()));
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_RSHIFT,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::SHIFT_RIGHT),
      next_time_stamp()));

  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_ESCAPE,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::ESCAPE),
      next_time_stamp()));
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_ESCAPE,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::ESCAPE),
      next_time_stamp()));

  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_A,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::US_A),
      next_time_stamp()));
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_A,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::US_A),
      next_time_stamp()));
}

}  // namespace ui
