// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_manager.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/test/testing_cursor_client_observer.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "ui/aura/env.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

class TestingCursorManager : public wm::NativeCursorManager {
 public:
  // Overridden from wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {}

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitCursor(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitMouseEventsEnabled(enabled);
  }

  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitCursorSize(cursor_size);
  }

  void SetLargeCursorSizeInDip(
      int large_cursor_size_in_dip,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitLargeCursorSizeInDip(large_cursor_size_in_dip);
  }

  void SetCursorColor(SkColor color,
                      wm::NativeCursorManagerDelegate* delegate) override {
    NOTIMPLEMENTED();
  }
};

}  // namespace

class CursorManagerTest : public aura::test::AuraTestBase {
 protected:
  CursorManagerTest()
      : cursor_manager_(std::make_unique<TestingCursorManager>()) {}

  wm::CursorManager cursor_manager_;
};

TEST_F(CursorManagerTest, ShowHideCursor) {
  cursor_manager_.SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  // Check if cursor visibility is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  // Checks setting visiblity while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.HideCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  cursor_manager_.ShowCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  cursor_manager_.ShowCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.HideCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
}

// Verifies that LockCursor/UnlockCursor work correctly with
// EnableMouseEvents and DisableMouseEvents
TEST_F(CursorManagerTest, EnableDisableMouseEvents) {
  cursor_manager_.SetCursor(ui::mojom::CursorType::kCopy);
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::mojom::CursorType::kCopy, cursor_manager_.GetCursor().type());

  // Check if cursor enable state is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // Checks enabling cursor while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.DisableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

TEST_F(CursorManagerTest, SetCursorSize) {
  wm::TestingCursorClientObserver observer;
  cursor_manager_.AddObserver(&observer);

  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kNormal, observer.cursor_size());

  cursor_manager_.SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kNormal, observer.cursor_size());

  cursor_manager_.SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_EQ(ui::CursorSize::kLarge, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kLarge, observer.cursor_size());

  cursor_manager_.SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_EQ(ui::CursorSize::kNormal, cursor_manager_.GetCursorSize());
  EXPECT_EQ(ui::CursorSize::kNormal, observer.cursor_size());
}

TEST_F(CursorManagerTest, IsMouseEventsEnabled) {
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that the mouse events enable state changes correctly when
// ShowCursor/HideCursor and EnableMouseEvents/DisableMouseEvents are used
// together.
TEST_F(CursorManagerTest, ShowAndEnable) {
  // Changing the visibility of the cursor does not affect the enable state.
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, it also gets invisible.
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are enabled, it restores the visibility state.
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.ShowCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.HideCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, ShowCursor is ignored.
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that calling DisableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
// This is a regression test for http://crbug.com/169404.
TEST_F(CursorManagerTest, MultipleDisableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

// Verifies that calling EnableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
TEST_F(CursorManagerTest, MultipleEnableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

TEST_F(CursorManagerTest, TestCursorClientObserver) {
  cursor_manager_.SetCursor(ui::mojom::CursorType::kPointer);
  // Add two observers. Both should have OnCursorVisibilityChanged()
  // invoked when the visibility of the cursor changes.
  wm::TestingCursorClientObserver observer_a;
  wm::TestingCursorClientObserver observer_b;
  cursor_manager_.AddObserver(&observer_a);
  cursor_manager_.AddObserver(&observer_b);

  // Initial state before any events have been sent.
  observer_a.reset();
  observer_b.reset();
  EXPECT_FALSE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());
  EXPECT_FALSE(observer_a.did_cursor_size_change());
  EXPECT_FALSE(observer_b.did_cursor_size_change());

  // Hide the cursor using HideCursor().
  cursor_manager_.HideCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
  EXPECT_FALSE(observer_b.is_cursor_visible());

  // Set the cursor size.
  cursor_manager_.SetCursorSize(ui::CursorSize::kLarge);
  EXPECT_TRUE(observer_a.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kLarge, observer_a.cursor_size());
  EXPECT_TRUE(observer_b.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kLarge, observer_b.cursor_size());

  // Show the cursor using ShowCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());
  EXPECT_TRUE(observer_b.is_cursor_visible());

  // Remove observer_b. Its OnCursorVisibilityChanged() should
  // not be invoked past this point.
  cursor_manager_.RemoveObserver(&observer_b);

  // Hide the cursor using HideCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.HideCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // Set back the cursor set to normal.
  cursor_manager_.SetCursorSize(ui::CursorSize::kNormal);
  EXPECT_TRUE(observer_a.did_cursor_size_change());
  EXPECT_EQ(ui::CursorSize::kNormal, observer_a.cursor_size());
  EXPECT_FALSE(observer_b.did_cursor_size_change());

  // Show the cursor using ShowCursor().
  observer_a.reset();
  observer_b.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_b.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());

  // Hide the cursor by changing the cursor type.
  cursor_manager_.SetCursor(ui::mojom::CursorType::kPointer);
  observer_a.reset();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kNone);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // Show the cursor by changing the cursor type.
  observer_a.reset();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kPointer);
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_TRUE(observer_a.is_cursor_visible());

  // Changing the type to another visible type doesn't cause unnecessary
  // callbacks.
  observer_a.reset();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kHand);
  EXPECT_FALSE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());

  // If the type is kNone, showing the cursor shouldn't cause observers to
  // think that the cursor is now visible.
  cursor_manager_.HideCursor();
  cursor_manager_.SetCursor(ui::mojom::CursorType::kNone);
  observer_a.reset();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(observer_a.did_visibility_change());
  EXPECT_FALSE(observer_a.is_cursor_visible());
}

#if BUILDFLAG(IS_WIN)
TEST_F(CursorManagerTest, SystemCursorVisibilityTest) {
  // System cursor visibility uses LockCursor()/UnlockCursor() to implement its
  // behaviour. Make sure this does not crash when
  // CommitSystemCursorVisibility(true) is called firstly. See
  // crbug.com/380703583.
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  cursor_manager_.UpdateSystemCursorVisibilityForTest(true);
  cursor_manager_.ShowCursor();
  // If the system cursor is invisible, ShowCursor() should not make the
  // cursor visible.
  cursor_manager_.UpdateSystemCursorVisibilityForTest(false);
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsCursorLocked());
}
#endif

// This test validates that the cursor visibility state is restored when a
// CursorManager instance is destroyed and recreated.
TEST(CursorManagerCreateDestroyTest, VisibilityTest) {
  // This block ensures that the cursor is hidden when the CursorManager
  // instance is destroyed.
  {
    wm::CursorManager cursor_manager1(
        base::WrapUnique(new TestingCursorManager));
    cursor_manager1.ShowCursor();
    EXPECT_TRUE(cursor_manager1.IsCursorVisible());
    cursor_manager1.HideCursor();
    EXPECT_FALSE(cursor_manager1.IsCursorVisible());
  }

  // This block validates that the cursor is hidden initially. It ensures that
  // the cursor is visible when the CursorManager instance is destroyed.
  {
    wm::CursorManager cursor_manager2(
        base::WrapUnique(new TestingCursorManager));
    EXPECT_FALSE(cursor_manager2.IsCursorVisible());
    cursor_manager2.ShowCursor();
    EXPECT_TRUE(cursor_manager2.IsCursorVisible());
  }

  // This block validates that the cursor is visible initially. It then
  // performs normal cursor visibility operations.
  {
    wm::CursorManager cursor_manager3(
        base::WrapUnique(new TestingCursorManager));
    EXPECT_TRUE(cursor_manager3.IsCursorVisible());
    cursor_manager3.HideCursor();
    EXPECT_FALSE(cursor_manager3.IsCursorVisible());
  }
}

#if BUILDFLAG(IS_WIN)
namespace {

// A TextInputClient whose editability can be controlled.
class FakeTextInputClient : public ui::DummyTextInputClient {
 public:
  explicit FakeTextInputClient(ui::TextInputType text_input_type)
      : ui::DummyTextInputClient(text_input_type) {}
};

}  // namespace

class CursorManagerHideOnTypingTest : public CursorManagerTest {
 public:
  CursorManagerHideOnTypingTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kHideCursorWhileTyping);
  }

  void SetUp() override {
    CursorManagerTest::SetUp();

    // Make Env::IsMouseButtonDown() consult the test-controlled mouse button
    // flags rather than the live OS state.
    aura::test::EnvTestHelper(aura::Env::GetInstance())
        .SetInputStateLookup(nullptr);
    aura::Env::GetInstance()->set_mouse_button_flags(0);

    cursor_manager_.SetMouseVanishEnabledForTesting(true);
    cursor_manager_.ShowCursor();
    ASSERT_TRUE(cursor_manager_.IsCursorVisible());

    host()->GetInputMethod()->SetFocusedTextInputClient(&text_input_client_);
  }

  void TearDown() override {
    host()->GetInputMethod()->DetachTextInputClient(&text_input_client_);
    CursorManagerTest::TearDown();
  }

 protected:
  ui::KeyEvent MakeKeyEvent(ui::EventType type,
                            ui::KeyboardCode key_code,
                            ui::DomCode code,
                            int flags,
                            ui::DomKey key) {
    ui::KeyEvent event(type, key_code, code, flags, key, ui::EventTimeForNow());
    ui::Event::DispatcherApi(&event).set_target(root_window());
    return event;
  }

  ui::KeyEvent MakeCharKeyPress(ui::KeyboardCode key_code,
                                ui::DomCode code,
                                char16_t character,
                                int flags = ui::EF_NONE) {
    return MakeKeyEvent(ui::EventType::kKeyPressed, key_code, code, flags,
                        ui::DomKey::FromCharacter(character));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  FakeTextInputClient text_input_client_{ui::TEXT_INPUT_TYPE_TEXT};
};

// A character key-press in an editable field hides the cursor.
TEST_F(CursorManagerHideOnTypingTest, HidesOnCharacterKeyPress) {
  ui::KeyEvent event = MakeCharKeyPress(ui::VKEY_A, ui::DomCode::US_A, u'a');
  EXPECT_TRUE(cursor_manager_.ShouldHideCursorOnKeyEvent(event));
}

// Only key-down events count as typing; key-up events must not hide.
TEST_F(CursorManagerHideOnTypingTest, DoesNotHideOnKeyRelease) {
  ui::KeyEvent event =
      MakeKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_A, ui::DomCode::US_A,
                   ui::EF_NONE, ui::DomKey::FromCharacter(u'a'));
  EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event));
}

TEST_F(CursorManagerHideOnTypingTest, DoesNotHideOnModifierOnlyKey) {
  const struct {
    ui::KeyboardCode key_code;
    ui::DomCode code;
    int flags;
    ui::DomKey key;
  } kModifierKeys[] = {
      {ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT, ui::EF_SHIFT_DOWN,
       ui::DomKey::SHIFT},
      {ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT, ui::EF_CONTROL_DOWN,
       ui::DomKey::CONTROL},
      {ui::VKEY_MENU, ui::DomCode::ALT_LEFT, ui::EF_ALT_DOWN, ui::DomKey::ALT},
  };
  for (const auto& modifier : kModifierKeys) {
    ui::KeyEvent event =
        MakeKeyEvent(ui::EventType::kKeyPressed, modifier.key_code,
                     modifier.code, modifier.flags, modifier.key);
    EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event))
        << "key_code=" << modifier.key_code;
  }
}

TEST_F(CursorManagerHideOnTypingTest, DoesNotHideOnFunctionKeys) {
  const struct {
    ui::KeyboardCode key_code;
    ui::DomCode code;
    ui::DomKey key;
  } kFunctionKeys[] = {
      {ui::VKEY_F1, ui::DomCode::F1, ui::DomKey::F1},
      {ui::VKEY_F5, ui::DomCode::F5, ui::DomKey::F5},
      {ui::VKEY_F12, ui::DomCode::F12, ui::DomKey::F12},
  };
  for (const auto& function_key : kFunctionKeys) {
    ui::KeyEvent event =
        MakeKeyEvent(ui::EventType::kKeyPressed, function_key.key_code,
                     function_key.code, ui::EF_NONE, function_key.key);
    EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event))
        << "key_code=" << function_key.key_code;
  }
}

// Navigation/whitespace keys (arrows, Esc, Tab) must not hide the cursor.
TEST_F(CursorManagerHideOnTypingTest, DoesNotHideOnNavigationKeys) {
  const struct {
    ui::KeyboardCode key_code;
    ui::DomCode code;
    ui::DomKey key;
  } kNavigationKeys[] = {
      {ui::VKEY_LEFT, ui::DomCode::ARROW_LEFT, ui::DomKey::ARROW_LEFT},
      {ui::VKEY_RIGHT, ui::DomCode::ARROW_RIGHT, ui::DomKey::ARROW_RIGHT},
      {ui::VKEY_UP, ui::DomCode::ARROW_UP, ui::DomKey::ARROW_UP},
      {ui::VKEY_DOWN, ui::DomCode::ARROW_DOWN, ui::DomKey::ARROW_DOWN},
      {ui::VKEY_ESCAPE, ui::DomCode::ESCAPE, ui::DomKey::ESCAPE},
  };
  for (const auto& navigation_key : kNavigationKeys) {
    ui::KeyEvent event =
        MakeKeyEvent(ui::EventType::kKeyPressed, navigation_key.key_code,
                     navigation_key.code, ui::EF_NONE, navigation_key.key);
    EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event))
        << "key_code=" << navigation_key.key_code;
  }
}

TEST_F(CursorManagerHideOnTypingTest, DoesNotHideOnKeyboardShortcut) {
  const int kModifierFlags[] = {ui::EF_CONTROL_DOWN, ui::EF_ALT_DOWN,
                                ui::EF_COMMAND_DOWN};
  for (int flags : kModifierFlags) {
    ui::KeyEvent event =
        MakeCharKeyPress(ui::VKEY_A, ui::DomCode::US_A, u'a', flags);
    EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event))
        << "flags=" << flags;
  }
}

TEST_F(CursorManagerHideOnTypingTest, DoesNotHideWhileMouseButtonDown) {
  aura::Env::GetInstance()->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  ui::KeyEvent event = MakeCharKeyPress(ui::VKEY_A, ui::DomCode::US_A, u'a');
  EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event));
}

// A non-editable focus (TEXT_INPUT_TYPE_NONE) must not hide the cursor.
TEST_F(CursorManagerHideOnTypingTest, DoesNotHideWhenInputTypeNone) {
  FakeTextInputClient none_client(ui::TEXT_INPUT_TYPE_NONE);
  host()->GetInputMethod()->SetFocusedTextInputClient(&none_client);

  ui::KeyEvent event = MakeCharKeyPress(ui::VKEY_A, ui::DomCode::US_A, u'a');
  EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event));

  host()->GetInputMethod()->DetachTextInputClient(&none_client);
}

TEST_F(CursorManagerHideOnTypingTest, DoesNotHideWhenCursorAlreadyHidden) {
  cursor_manager_.HideCursor();
  ASSERT_FALSE(cursor_manager_.IsCursorVisible());

  ui::KeyEvent event = MakeCharKeyPress(ui::VKEY_A, ui::DomCode::US_A, u'a');
  EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event));
}

TEST_F(CursorManagerHideOnTypingTest, DoesNotHideWhenSystemSettingDisabled) {
  cursor_manager_.SetMouseVanishEnabledForTesting(false);
  ui::KeyEvent event = MakeCharKeyPress(ui::VKEY_A, ui::DomCode::US_A, u'a');
  EXPECT_FALSE(cursor_manager_.ShouldHideCursorOnKeyEvent(event));
}
#endif  // BUILDFLAG(IS_WIN)
