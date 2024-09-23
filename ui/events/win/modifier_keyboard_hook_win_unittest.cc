// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/keyboard_layout.h"
#include "ui/events/win/keyboard_hook_win_base.h"
#include "ui/events/win/system_event_state_lookup.h"

namespace ui {

class ModifierKeyboardHookWinTest : public testing::Test {
 public:
  ModifierKeyboardHookWinTest();

  ModifierKeyboardHookWinTest(const ModifierKeyboardHookWinTest&) = delete;
  ModifierKeyboardHookWinTest& operator=(const ModifierKeyboardHookWinTest&) =
      delete;

  ~ModifierKeyboardHookWinTest() override;

  // testing::Test overrides.
  void SetUp() override;

  void HandleKeyPress(KeyEvent* key_event);

 protected:
  KeyboardHookWinBase* keyboard_hook() { return keyboard_hook_.get(); }

  uint32_t next_time_stamp() { return time_stamp_++; }

  std::vector<KeyEvent>* key_events() { return &key_events_; }

  // Used for sending key events which are handled by the hook.
  void SendModifierKeyDownEvent(KeyboardCode key_code,
                                DomCode dom_code,
                                int repeat_count = 1);
  void SendModifierKeyUpEvent(KeyboardCode key_code, DomCode dom_code);

  void SetKeyboardLayoutForTest(KeyboardLayout new_layout);

 private:
  uint32_t time_stamp_ = 0;
  std::unique_ptr<KeyboardHookWinBase> keyboard_hook_;
  std::vector<KeyEvent> key_events_;
  std::unique_ptr<ScopedKeyboardLayout> keyboard_layout_;
};

ModifierKeyboardHookWinTest::ModifierKeyboardHookWinTest() = default;

ModifierKeyboardHookWinTest::~ModifierKeyboardHookWinTest() = default;

void ModifierKeyboardHookWinTest::SetUp() {
  keyboard_hook_ = KeyboardHookWinBase::CreateModifierKeyboardHookForTesting(
      std::optional<base::flat_set<DomCode>>(),
      base::BindRepeating(&ModifierKeyboardHookWinTest::HandleKeyPress,
                          base::Unretained(this)));

  keyboard_layout_ = std::make_unique<ScopedKeyboardLayout>(
      KeyboardLayout::KEYBOARD_LAYOUT_ENGLISH_US);
}

void ModifierKeyboardHookWinTest::HandleKeyPress(KeyEvent* key_event) {
  key_events_.push_back(*key_event);
}

void ModifierKeyboardHookWinTest::SendModifierKeyDownEvent(
    KeyboardCode key_code,
    DomCode dom_code,
    int repeat_count /*=1*/) {
  // Ensure we have a valid repeat count and the modifer passed in contains
  // location information.
  DCHECK_GT(repeat_count, 0);
  DCHECK_NE(key_code, KeyboardCode::VKEY_CONTROL);
  DCHECK_NE(key_code, KeyboardCode::VKEY_MENU);

  for (int i = 0; i < repeat_count; i++) {
    ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
        WM_KEYDOWN, key_code,
        KeycodeConverter::DomCodeToNativeKeycode(dom_code), next_time_stamp()));
  }
}

void ModifierKeyboardHookWinTest::SendModifierKeyUpEvent(KeyboardCode key_code,
                                                         DomCode dom_code) {
  // Ensure we have a valid repeat count and the modifer passed in contains
  // location information.
  DCHECK_NE(key_code, KeyboardCode::VKEY_CONTROL);
  DCHECK_NE(key_code, KeyboardCode::VKEY_MENU);

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

TEST_F(ModifierKeyboardHookWinTest, SimpleLeftControlKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_LCONTROL;
  const DomCode dom_code = DomCode::CONTROL_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1ULL);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2ULL);

  KeyEvent down_event = key_events()->at(0);
  VerifyKeyEvent(&down_event, KeyboardCode::VKEY_CONTROL, dom_code, true,
                 false);
  ASSERT_TRUE(down_event.IsControlDown());
  ASSERT_FALSE(down_event.IsAltDown());
  ASSERT_FALSE(down_event.IsCommandDown());

  KeyEvent up_event = key_events()->at(1);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_CONTROL, dom_code, false, false);
  ASSERT_FALSE(up_event.IsControlDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingLeftControlKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_LCONTROL;
  const DomCode dom_code = DomCode::CONTROL_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, dom_code, true,
                   should_repeat);
    ASSERT_TRUE(event.IsControlDown());
    ASSERT_FALSE(event.IsAltDown());
    ASSERT_FALSE(event.IsCommandDown());
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_CONTROL, dom_code, false, false);
  ASSERT_FALSE(up_event.IsControlDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleRightControlKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_RCONTROL;
  const DomCode dom_code = DomCode::CONTROL_RIGHT;
  SendModifierKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1ULL);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2ULL);

  KeyEvent down_event = key_events()->at(0);
  VerifyKeyEvent(&down_event, KeyboardCode::VKEY_CONTROL, dom_code, true,
                 false);
  ASSERT_TRUE(down_event.IsControlDown());
  ASSERT_FALSE(down_event.IsAltDown());
  ASSERT_FALSE(down_event.IsCommandDown());

  KeyEvent up_event = key_events()->at(1);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_CONTROL, dom_code, false, false);
  ASSERT_FALSE(up_event.IsControlDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingRightControlKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_RCONTROL;
  const DomCode dom_code = DomCode::CONTROL_RIGHT;
  SendModifierKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, dom_code, true,
                   should_repeat);
    ASSERT_TRUE(event.IsControlDown());
    ASSERT_FALSE(event.IsAltDown());
    ASSERT_FALSE(event.IsCommandDown());
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_CONTROL, dom_code, false, false);
  ASSERT_FALSE(up_event.IsControlDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleLifoControlSequenceTest) {
  const KeyboardCode left_key_code = KeyboardCode::VKEY_LCONTROL;
  const DomCode left_dom_code = DomCode::CONTROL_LEFT;
  const KeyboardCode right_key_code = KeyboardCode::VKEY_RCONTROL;
  const DomCode right_dom_code = DomCode::CONTROL_RIGHT;
  SendModifierKeyDownEvent(left_key_code, left_dom_code, 2);
  SendModifierKeyDownEvent(right_key_code, right_dom_code, 2);
  SendModifierKeyUpEvent(right_key_code, right_dom_code);
  SendModifierKeyUpEvent(left_key_code, left_dom_code);
  ASSERT_EQ(key_events()->size(), 6ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, left_dom_code, true,
                 false);
  ASSERT_TRUE(event.IsControlDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, left_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());

  // Second key down, repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, right_dom_code, true,
                 true);
  ASSERT_TRUE(event.IsControlDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, right_dom_code, true,
                 true);
  ASSERT_TRUE(event.IsControlDown());

  // Second key up.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, right_dom_code, false,
                 false);
  ASSERT_TRUE(event.IsControlDown());

  // First key up.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, left_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleFifoControlSequenceTest) {
  const KeyboardCode left_key_code = KeyboardCode::VKEY_LCONTROL;
  const DomCode left_dom_code = DomCode::CONTROL_LEFT;
  const KeyboardCode right_key_code = KeyboardCode::VKEY_RCONTROL;
  const DomCode right_dom_code = DomCode::CONTROL_RIGHT;
  SendModifierKeyDownEvent(right_key_code, right_dom_code, 2);
  SendModifierKeyDownEvent(left_key_code, left_dom_code, 2);
  SendModifierKeyUpEvent(right_key_code, right_dom_code);
  SendModifierKeyUpEvent(left_key_code, left_dom_code);
  ASSERT_EQ(key_events()->size(), 6ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, right_dom_code, true,
                 false);
  ASSERT_TRUE(event.IsControlDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, right_dom_code, true,
                 true);
  ASSERT_TRUE(event.IsControlDown());

  // Second key down, repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, left_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, left_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());

  // First key up.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, right_dom_code, false,
                 false);
  ASSERT_TRUE(event.IsControlDown());

  // Second key up.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, left_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleLeftAltKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_LMENU;
  const DomCode dom_code = DomCode::ALT_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1ULL);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2ULL);

  KeyEvent down_event = key_events()->at(0);
  VerifyKeyEvent(&down_event, KeyboardCode::VKEY_MENU, dom_code, true, false);
  ASSERT_FALSE(down_event.IsControlDown());
  ASSERT_TRUE(down_event.IsAltDown());
  ASSERT_FALSE(down_event.IsCommandDown());

  KeyEvent up_event = key_events()->at(1);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_MENU, dom_code, false, false);
  ASSERT_FALSE(up_event.IsAltDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingLeftAltKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_LMENU;
  const DomCode dom_code = DomCode::ALT_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, dom_code, true,
                   should_repeat);
    ASSERT_FALSE(event.IsControlDown());
    ASSERT_TRUE(event.IsAltDown());
    ASSERT_FALSE(event.IsCommandDown());
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_MENU, dom_code, false, false);
  ASSERT_FALSE(up_event.IsAltDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleRightAltKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_RMENU;
  const DomCode dom_code = DomCode::ALT_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1ULL);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2ULL);

  KeyEvent down_event = key_events()->at(0);
  VerifyKeyEvent(&down_event, KeyboardCode::VKEY_MENU, dom_code, true, false);
  ASSERT_FALSE(down_event.IsControlDown());
  ASSERT_TRUE(down_event.IsAltDown());
  ASSERT_FALSE(down_event.IsCommandDown());

  KeyEvent up_event = key_events()->at(1);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_MENU, dom_code, false, false);
  ASSERT_FALSE(up_event.IsAltDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingRightAltKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_RMENU;
  const DomCode dom_code = DomCode::ALT_RIGHT;
  SendModifierKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, dom_code, true,
                   should_repeat);
    ASSERT_FALSE(event.IsControlDown());
    ASSERT_TRUE(event.IsAltDown());
    ASSERT_FALSE(event.IsCommandDown());
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_MENU, dom_code, false, false);
  ASSERT_FALSE(up_event.IsAltDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleLifoAltSequenceTest) {
  const KeyboardCode left_key_code = KeyboardCode::VKEY_LMENU;
  const DomCode left_dom_code = DomCode::ALT_LEFT;
  const KeyboardCode right_key_code = KeyboardCode::VKEY_RMENU;
  const DomCode right_dom_code = DomCode::ALT_RIGHT;
  SendModifierKeyDownEvent(left_key_code, left_dom_code, 2);
  SendModifierKeyDownEvent(right_key_code, right_dom_code, 2);
  SendModifierKeyUpEvent(right_key_code, right_dom_code);
  SendModifierKeyUpEvent(left_key_code, left_dom_code);
  ASSERT_EQ(key_events()->size(), 6ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, left_dom_code, true, false);
  ASSERT_TRUE(event.IsAltDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, left_dom_code, true, true);
  ASSERT_TRUE(event.IsAltDown());

  // Second key down, repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, right_dom_code, true, true);
  ASSERT_TRUE(event.IsAltDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, right_dom_code, true, true);
  ASSERT_TRUE(event.IsAltDown());

  // Second key up.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, right_dom_code, false, false);
  ASSERT_TRUE(event.IsAltDown());

  // First key up.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, left_dom_code, false, false);
  ASSERT_FALSE(event.IsAltDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleFifoAltSequenceTest) {
  const KeyboardCode left_key_code = KeyboardCode::VKEY_LMENU;
  const DomCode left_dom_code = DomCode::ALT_LEFT;
  const KeyboardCode right_key_code = KeyboardCode::VKEY_RMENU;
  const DomCode right_dom_code = DomCode::ALT_RIGHT;
  SendModifierKeyDownEvent(right_key_code, right_dom_code, 2);
  SendModifierKeyDownEvent(left_key_code, left_dom_code, 2);
  SendModifierKeyUpEvent(right_key_code, right_dom_code);
  SendModifierKeyUpEvent(left_key_code, left_dom_code);
  ASSERT_EQ(key_events()->size(), 6ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, right_dom_code, true, false);
  ASSERT_TRUE(event.IsAltDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, right_dom_code, true, true);
  ASSERT_TRUE(event.IsAltDown());

  // Second key down, repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, left_dom_code, true, true);
  ASSERT_TRUE(event.IsAltDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, left_dom_code, true, true);
  ASSERT_TRUE(event.IsAltDown());

  // First key up.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, right_dom_code, false, false);
  ASSERT_TRUE(event.IsAltDown());

  // Second key up.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, left_dom_code, false, false);
  ASSERT_FALSE(event.IsAltDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleLeftWinKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_LWIN;
  const DomCode dom_code = DomCode::META_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1ULL);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2ULL);

  KeyEvent down_event = key_events()->at(0);
  // VKEY_LWIN is the 'non-located' version of the Windows key.
  VerifyKeyEvent(&down_event, KeyboardCode::VKEY_LWIN, dom_code, true, false);
  ASSERT_FALSE(down_event.IsControlDown());
  ASSERT_FALSE(down_event.IsAltDown());
  ASSERT_TRUE(down_event.IsCommandDown());

  KeyEvent up_event = key_events()->at(1);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_LWIN, dom_code, false, false);
  ASSERT_FALSE(up_event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingLeftWinKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_LWIN;
  const DomCode dom_code = DomCode::META_LEFT;
  SendModifierKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    // VKEY_LWIN is the 'non-located' version of the Windows key.
    VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, dom_code, true,
                   should_repeat);
    ASSERT_FALSE(event.IsControlDown());
    ASSERT_FALSE(event.IsAltDown());
    ASSERT_TRUE(event.IsCommandDown());
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_LWIN, dom_code, false, false);
  ASSERT_FALSE(up_event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleRightWinKeypressTest) {
  const KeyboardCode key_code = KeyboardCode::VKEY_RWIN;
  const DomCode dom_code = DomCode::META_RIGHT;
  SendModifierKeyDownEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 1ULL);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(key_events()->size(), 2ULL);

  KeyEvent down_event = key_events()->at(0);
  // VKEY_LWIN is the 'non-located' version of the Windows key.
  VerifyKeyEvent(&down_event, KeyboardCode::VKEY_RWIN, dom_code, true, false);
  ASSERT_FALSE(down_event.IsControlDown());
  ASSERT_FALSE(down_event.IsAltDown());
  ASSERT_TRUE(down_event.IsCommandDown());

  KeyEvent up_event = key_events()->at(1);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_RWIN, dom_code, false, false);
  ASSERT_FALSE(up_event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingRightWinKeypressTest) {
  const int repeat_count = 10;
  const KeyboardCode key_code = KeyboardCode::VKEY_RWIN;
  const DomCode dom_code = DomCode::META_RIGHT;
  SendModifierKeyDownEvent(key_code, dom_code, repeat_count);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count);
  SendModifierKeyUpEvent(key_code, dom_code);
  ASSERT_EQ(static_cast<int>(key_events()->size()), repeat_count + 1);

  bool should_repeat = false;
  for (int i = 0; i < repeat_count; i++) {
    KeyEvent event = key_events()->at(i);
    // VKEY_LWIN is the 'non-located' version of the Windows key.
    VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, dom_code, true,
                   should_repeat);
    ASSERT_FALSE(event.IsControlDown());
    ASSERT_FALSE(event.IsAltDown());
    ASSERT_TRUE(event.IsCommandDown());
    should_repeat = true;
  }

  KeyEvent up_event = key_events()->at(repeat_count);
  VerifyKeyEvent(&up_event, KeyboardCode::VKEY_RWIN, dom_code, false, false);
  ASSERT_FALSE(up_event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleLifoWinSequenceTest) {
  const KeyboardCode left_key_code = KeyboardCode::VKEY_LWIN;
  const DomCode left_dom_code = DomCode::META_LEFT;
  const KeyboardCode right_key_code = KeyboardCode::VKEY_RWIN;
  const DomCode right_dom_code = DomCode::META_RIGHT;
  SendModifierKeyDownEvent(left_key_code, left_dom_code, 2);
  SendModifierKeyDownEvent(right_key_code, right_dom_code, 2);
  SendModifierKeyUpEvent(right_key_code, right_dom_code);
  SendModifierKeyUpEvent(left_key_code, left_dom_code);
  ASSERT_EQ(key_events()->size(), 6ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, left_dom_code, true, false);
  ASSERT_TRUE(event.IsCommandDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, left_dom_code, true, true);
  ASSERT_TRUE(event.IsCommandDown());

  // Second key down, no repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, right_dom_code, true, false);
  ASSERT_TRUE(event.IsCommandDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, right_dom_code, true, true);
  ASSERT_TRUE(event.IsCommandDown());

  // Second key up.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, right_dom_code, false, false);
  ASSERT_TRUE(event.IsCommandDown());

  // First key up.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, left_dom_code, false, false);
  ASSERT_FALSE(event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleFifoWinSequenceTest) {
  const KeyboardCode left_key_code = KeyboardCode::VKEY_LWIN;
  const DomCode left_dom_code = DomCode::META_LEFT;
  const KeyboardCode right_key_code = KeyboardCode::VKEY_RWIN;
  const DomCode right_dom_code = DomCode::META_RIGHT;
  SendModifierKeyDownEvent(right_key_code, right_dom_code, 2);
  SendModifierKeyDownEvent(left_key_code, left_dom_code, 2);
  SendModifierKeyUpEvent(right_key_code, right_dom_code);
  SendModifierKeyUpEvent(left_key_code, left_dom_code);
  ASSERT_EQ(key_events()->size(), 6ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, right_dom_code, true, false);
  ASSERT_TRUE(event.IsCommandDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, right_dom_code, true, true);
  ASSERT_TRUE(event.IsCommandDown());

  // Second key down, no repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, left_dom_code, true, false);
  ASSERT_TRUE(event.IsCommandDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, left_dom_code, true, true);
  ASSERT_TRUE(event.IsCommandDown());

  // First key up.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, right_dom_code, false, false);
  ASSERT_TRUE(event.IsCommandDown());

  // Second key up.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, left_dom_code, false, false);
  ASSERT_FALSE(event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, CombinedModifierLifoSequenceKeypressTest) {
  const KeyboardCode first_key_code = KeyboardCode::VKEY_LCONTROL;
  const DomCode first_dom_code = DomCode::CONTROL_LEFT;
  const KeyboardCode second_key_code = KeyboardCode::VKEY_RWIN;
  const DomCode second_dom_code = DomCode::META_RIGHT;
  const KeyboardCode third_key_code = KeyboardCode::VKEY_LMENU;
  const DomCode third_dom_code = DomCode::ALT_LEFT;
  SendModifierKeyDownEvent(first_key_code, first_dom_code, 2);
  SendModifierKeyDownEvent(second_key_code, second_dom_code, 2);
  SendModifierKeyDownEvent(third_key_code, third_dom_code, 2);
  SendModifierKeyUpEvent(third_key_code, third_dom_code);
  SendModifierKeyUpEvent(second_key_code, second_dom_code);
  SendModifierKeyUpEvent(first_key_code, first_dom_code);
  ASSERT_EQ(key_events()->size(), 9ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, first_dom_code, true,
                 false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, first_dom_code, true,
                 true);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Second key down, no repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, second_dom_code, true, false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, second_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Third key down, no repeat.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, third_dom_code, true, false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_TRUE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Third key still down, repeat.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, third_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_TRUE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Third key up.
  event = key_events()->at(6);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, third_dom_code, false, false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Second key up.
  event = key_events()->at(7);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_RWIN, second_dom_code, false,
                 false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());

  // First key up.
  event = key_events()->at(8);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, first_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, CombinedModifierFifoSequenceKeypressTest) {
  const KeyboardCode first_key_code = KeyboardCode::VKEY_RCONTROL;
  const DomCode first_dom_code = DomCode::CONTROL_RIGHT;
  const KeyboardCode second_key_code = KeyboardCode::VKEY_LWIN;
  const DomCode second_dom_code = DomCode::META_LEFT;
  const KeyboardCode third_key_code = KeyboardCode::VKEY_RMENU;
  const DomCode third_dom_code = DomCode::ALT_RIGHT;
  SendModifierKeyDownEvent(first_key_code, first_dom_code, 2);
  SendModifierKeyDownEvent(second_key_code, second_dom_code, 2);
  SendModifierKeyDownEvent(third_key_code, third_dom_code, 2);
  SendModifierKeyUpEvent(first_key_code, first_dom_code);
  SendModifierKeyUpEvent(second_key_code, second_dom_code);
  SendModifierKeyUpEvent(third_key_code, third_dom_code);
  ASSERT_EQ(key_events()->size(), 9ULL);

  // First key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, first_dom_code, true,
                 false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());

  // First key still down, repeat.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, first_dom_code, true,
                 true);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Second key down, no repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, second_dom_code, true, false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Second key still down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, second_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Third key down, no repeat.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, third_dom_code, true, false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_TRUE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Third key still down, repeat.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, third_dom_code, true, true);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_TRUE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // First key up.
  event = key_events()->at(6);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, first_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_TRUE(event.IsAltDown());
  ASSERT_TRUE(event.IsCommandDown());

  // Second key up.
  event = key_events()->at(7);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_LWIN, second_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_TRUE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Third key up.
  event = key_events()->at(8);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, third_dom_code, false, false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, VerifyPlatformModifierStateTest) {
  SendModifierKeyDownEvent(KeyboardCode::VKEY_LCONTROL, DomCode::CONTROL_LEFT);
  ASSERT_TRUE(win::IsCtrlPressed());
  ASSERT_FALSE(win::IsAltPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  SendModifierKeyDownEvent(KeyboardCode::VKEY_RWIN, DomCode::META_RIGHT);
  ASSERT_TRUE(win::IsCtrlPressed());
  ASSERT_FALSE(win::IsAltPressed());
  ASSERT_TRUE(win::IsWindowsKeyPressed());

  SendModifierKeyDownEvent(KeyboardCode::VKEY_LMENU, DomCode::ALT_LEFT);
  ASSERT_TRUE(win::IsCtrlPressed());
  ASSERT_TRUE(win::IsAltPressed());
  ASSERT_FALSE(win::IsAltRightPressed());
  ASSERT_TRUE(win::IsWindowsKeyPressed());

  SendModifierKeyUpEvent(KeyboardCode::VKEY_RWIN, DomCode::META_RIGHT);
  ASSERT_TRUE(win::IsCtrlPressed());
  ASSERT_TRUE(win::IsAltPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  SendModifierKeyUpEvent(KeyboardCode::VKEY_LCONTROL, DomCode::CONTROL_LEFT);
  ASSERT_FALSE(win::IsCtrlPressed());
  ASSERT_TRUE(win::IsAltPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  SendModifierKeyUpEvent(KeyboardCode::VKEY_LMENU, DomCode::ALT_LEFT);
  ASSERT_FALSE(win::IsCtrlPressed());
  ASSERT_FALSE(win::IsAltPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  SendModifierKeyDownEvent(KeyboardCode::VKEY_RMENU, DomCode::ALT_RIGHT);
  ASSERT_TRUE(win::IsAltPressed());
  ASSERT_TRUE(win::IsAltRightPressed());
}

TEST_F(ModifierKeyboardHookWinTest, SimpleAltGrKeyPressTest) {
  ScopedKeyboardLayout keyboard_layout(KeyboardLayout::KEYBOARD_LAYOUT_GERMAN);

  // AltGr produces two events, an injected, modified scan code for VK_LCONTROL,
  // and an event for VK_RMENU.  We simulate that sequence here.
  const KeyboardCode altgr_key_code = KeyboardCode::VKEY_RMENU;
  const DomCode altgr_dom_code = DomCode::ALT_RIGHT;
  const DomCode control_dom_code = DomCode::CONTROL_LEFT;
  const DWORD control_scan_code =
      KeycodeConverter::DomCodeToNativeKeycode(control_dom_code);
  const DWORD injected_control_scan_code = control_scan_code | 0x0200;

  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_LCONTROL, injected_control_scan_code,
      next_time_stamp()));
  SendModifierKeyDownEvent(altgr_key_code, altgr_dom_code);

  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_LCONTROL,
      KeycodeConverter::DomCodeToNativeKeycode(control_dom_code),
      next_time_stamp()));
  SendModifierKeyUpEvent(altgr_key_code, altgr_dom_code);
  ASSERT_EQ(key_events()->size(), 4ULL);

  // Injected control key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, control_dom_code, true,
                 false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Altgr key down.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, altgr_dom_code, true, false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Injected control key up.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, control_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // AltGr key up.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, altgr_dom_code, false, false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, RepeatingAltGrKeyPressTest) {
  ScopedKeyboardLayout keyboard_layout(KeyboardLayout::KEYBOARD_LAYOUT_GERMAN);

  // AltGr produces two events, an injected, modified scan code for VK_LCONTROL,
  // and an event for VK_RMENU.  This sequence repeats for each repeated key
  // press.  We simulate that sequence here.
  const KeyboardCode altgr_key_code = KeyboardCode::VKEY_RMENU;
  const DomCode altgr_dom_code = DomCode::ALT_RIGHT;
  const DomCode control_dom_code = DomCode::CONTROL_LEFT;
  const DWORD control_scan_code =
      KeycodeConverter::DomCodeToNativeKeycode(control_dom_code);
  const DWORD injected_control_scan_code = control_scan_code | 0x0200;

  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_LCONTROL, injected_control_scan_code,
      next_time_stamp()));
  SendModifierKeyDownEvent(altgr_key_code, altgr_dom_code);
  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_LCONTROL, injected_control_scan_code,
      next_time_stamp()));
  SendModifierKeyDownEvent(altgr_key_code, altgr_dom_code);
  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_LCONTROL, injected_control_scan_code,
      next_time_stamp()));
  SendModifierKeyDownEvent(altgr_key_code, altgr_dom_code);
  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_LCONTROL,
      KeycodeConverter::DomCodeToNativeKeycode(control_dom_code),
      next_time_stamp()));
  SendModifierKeyUpEvent(altgr_key_code, altgr_dom_code);
  ASSERT_EQ(key_events()->size(), 8ULL);

  // Injected control key down, no repeat.
  KeyEvent event = key_events()->at(0);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, control_dom_code, true,
                 false);
  ASSERT_TRUE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Altgr key down.
  event = key_events()->at(1);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, altgr_dom_code, true, false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Injected control key down, repeat.
  event = key_events()->at(2);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, control_dom_code, true,
                 true);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Altgr key down, repeat.
  event = key_events()->at(3);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, altgr_dom_code, true, true);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Injected control key down, repeat.
  event = key_events()->at(4);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, control_dom_code, true,
                 true);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Altgr key down, repeat.
  event = key_events()->at(5);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, altgr_dom_code, true, true);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // Injected control key up.
  event = key_events()->at(6);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_CONTROL, control_dom_code, false,
                 false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_TRUE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());

  // AltGr key up.
  event = key_events()->at(7);
  VerifyKeyEvent(&event, KeyboardCode::VKEY_MENU, altgr_dom_code, false, false);
  ASSERT_FALSE(event.IsControlDown());
  ASSERT_FALSE(event.IsAltDown());
  ASSERT_FALSE(event.IsAltGrDown());
  ASSERT_FALSE(event.IsCommandDown());
}

TEST_F(ModifierKeyboardHookWinTest, VerifyAltGrPlatformModifierStateTest) {
  ScopedKeyboardLayout keyboard_layout(KeyboardLayout::KEYBOARD_LAYOUT_GERMAN);

  // AltGr produces two events, an injected, modified scan code for VK_LCONTROL,
  // and an event for VK_RMENU.  We simulate that sequence here.
  const KeyboardCode altgr_key_code = KeyboardCode::VKEY_RMENU;
  const DomCode altgr_dom_code = DomCode::ALT_RIGHT;
  const DomCode control_dom_code = DomCode::CONTROL_LEFT;
  const DWORD control_scan_code =
      KeycodeConverter::DomCodeToNativeKeycode(control_dom_code);
  const DWORD injected_control_scan_code = control_scan_code | 0x0200;

  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYDOWN, KeyboardCode::VKEY_LCONTROL, injected_control_scan_code,
      next_time_stamp()));
  ASSERT_TRUE(win::IsCtrlPressed());
  ASSERT_FALSE(win::IsAltPressed());
  ASSERT_FALSE(win::IsAltRightPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  SendModifierKeyDownEvent(altgr_key_code, altgr_dom_code);
  ASSERT_TRUE(win::IsCtrlPressed());
  ASSERT_TRUE(win::IsAltPressed());
  ASSERT_TRUE(win::IsAltRightPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  ASSERT_TRUE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_LCONTROL,
      KeycodeConverter::DomCodeToNativeKeycode(control_dom_code),
      next_time_stamp()));
  ASSERT_FALSE(win::IsCtrlPressed());
  ASSERT_TRUE(win::IsAltPressed());
  ASSERT_TRUE(win::IsAltRightPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());

  SendModifierKeyUpEvent(altgr_key_code, altgr_dom_code);
  ASSERT_FALSE(win::IsCtrlPressed());
  ASSERT_FALSE(win::IsAltPressed());
  ASSERT_FALSE(win::IsAltRightPressed());
  ASSERT_FALSE(win::IsWindowsKeyPressed());
}

TEST_F(ModifierKeyboardHookWinTest, NonInterceptedKeysTest) {
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
      WM_KEYDOWN, KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::MEDIA_PLAY_PAUSE),
      next_time_stamp()));
  ASSERT_FALSE(keyboard_hook()->ProcessKeyEventMessage(
      WM_KEYUP, KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
      KeycodeConverter::DomCodeToNativeKeycode(DomCode::MEDIA_PLAY_PAUSE),
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
