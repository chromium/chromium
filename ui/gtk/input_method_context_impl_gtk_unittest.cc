// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/input_method_context_impl_gtk.h"

#include <glib.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/scoped_environment_variable_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gtk/gtk_ui_platform_stub.h"
#include "url/gurl.h"

namespace gtk {

namespace {

constexpr uint8_t kUnusedKeyCode = 248;
constexpr uint8_t kKeyACode = 38;
constexpr uint8_t kShiftLeftKeyCode = 50;
constexpr uint32_t kDeadAcuteKeySym = 0xfe51;  // XK_dead_acute
constexpr int32_t kCombiningAcute = 0x0301;

class TestInputMethodContextDelegate
    : public ui::LinuxInputMethodContextDelegate {
 public:
  gfx::AcceleratedWidget GetClientWindowKey() const override {
    return gfx::kNullAcceleratedWidget;
  }
  void OnCommit(const std::u16string& text) override {
    committed_text_ += text;
  }
  void OnConfirmCompositionText(bool keep_selection) override {}
  void OnDeleteSurroundingText(size_t before, size_t after) override {}
  void OnPreeditChanged(const ui::CompositionText& composition_text) override {}
  void OnPreeditEnd() override {}
  void OnPreeditStart() override {}
  void OnSetPreeditRegion(const gfx::Range& range,
                          const std::vector<ui::ImeTextSpan>& spans) override {}
  void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) override {}
  void OnInsertImage(const GURL& src) override {}

  const std::u16string& committed_text() const { return committed_text_; }
  void ClearCommittedText() { committed_text_.clear(); }

 private:
  std::u16string committed_text_;
};

ui::KeyEvent MakeX11KeyEvent(ui::EventType type,
                             ui::KeyboardCode key_code,
                             ui::DomCode dom_code,
                             ui::DomKey dom_key,
                             uint8_t hw_keycode,
                             uint32_t state = 0) {
  ui::KeyEvent event(type, key_code, dom_code, ui::EF_NONE, dom_key,
                     base::TimeTicks());
  ui::Event::Properties properties;
  properties.emplace(ui::kPropertyKeyboardHwKeyCode,
                     std::vector<uint8_t>{hw_keycode});
  properties.emplace(ui::kPropertyKeyboardGroup, std::vector<uint8_t>{0});
  properties.emplace(ui::kPropertyKeyboardState,
                     std::vector<uint8_t>{
                         static_cast<uint8_t>(state),
                         static_cast<uint8_t>(state >> 8),
                         static_cast<uint8_t>(state >> 16),
                         static_cast<uint8_t>(state >> 24),
                     });
  event.SetProperties(properties);
  return event;
}

void FlushGlibEvents() {
  while (g_main_context_iteration(nullptr, FALSE)) {
  }
}

base::ScopedClosureRunner MapDeadAcuteKeyCode() {
  auto* connection = x11::Connection::Get();
  auto orig_reply =
      connection
          ->GetKeyboardMapping({static_cast<x11::KeyCode>(kUnusedKeyCode), 1})
          .Sync();
  CHECK(orig_reply);

  connection
      ->ChangeKeyboardMapping(1, static_cast<x11::KeyCode>(kUnusedKeyCode), 1,
                              {static_cast<x11::KeySym>(kDeadAcuteKeySym)})
      .Sync();
  FlushGlibEvents();

  uint8_t orig_per_keycode = orig_reply->keysyms_per_keycode;
  std::vector<x11::KeySym> orig_syms = std::move(orig_reply->keysyms);
  return base::ScopedClosureRunner(base::BindOnce(
      [](uint8_t per_keycode, std::vector<x11::KeySym> syms) {
        if (per_keycode > 0) {
          x11::Connection::Get()
              ->ChangeKeyboardMapping(1,
                                      static_cast<x11::KeyCode>(kUnusedKeyCode),
                                      per_keycode, syms)
              .Sync();
          FlushGlibEvents();
        }
      },
      orig_per_keycode, std::move(orig_syms)));
}

}  // namespace

// Regression test for crbug.com/357905826: GtkIMContextSimple (both directly
// and as the fallback slave of GtkIMMulticontext / IBus-GTK) filters every
// KeyRelease while a dead-key compose sequence is active, which previously
// caused InputMethodContextImplGtk::DispatchKeyEvent() to report KeyRelease
// events as handled and drop DOM keyup events.
TEST(InputMethodContextImplGtkTest,
     DeadKeyDoesNotFilterKeyReleaseAndStillComposes) {
  base::ScopedClosureRunner restore_keymap = MapDeadAcuteKeyCode();

  for (const char* im_module : {"", "gtk-im-context-simple"}) {
    SCOPED_TRACE(im_module);
    std::unique_ptr<base::ScopedEnvironmentVariableOverride> im_override;
    if (*im_module) {
      im_override = std::make_unique<base::ScopedEnvironmentVariableOverride>(
          "GTK_IM_MODULE", im_module);
    }

    GtkUiPlatformStub platform;
    TestInputMethodContextDelegate delegate;
    InputMethodContextImplGtk context(&delegate, &platform);

    for (ui::TextInputType input_type :
         {ui::TEXT_INPUT_TYPE_NONE, ui::TEXT_INPUT_TYPE_TEXT}) {
      SCOPED_TRACE(input_type);
      delegate.ClearCommittedText();

      ui::LinuxInputMethodContext::TextInputClientAttributes attrs;
      attrs.input_type = input_type;
      context.UpdateFocus(/*has_client=*/true, ui::TEXT_INPUT_TYPE_NONE, attrs,
                          ui::TextInputClient::FOCUS_REASON_OTHER);

      ui::KeyEvent dead_press = MakeX11KeyEvent(
          ui::EventType::kKeyPressed, ui::VKEY_OEM_4, ui::DomCode::BRACKET_LEFT,
          ui::DomKey::DeadKeyFromCombiningCharacter(kCombiningAcute),
          kUnusedKeyCode);
      EXPECT_TRUE(context.DispatchKeyEvent(dead_press));
      EXPECT_TRUE(delegate.committed_text().empty());

      // Neither the dead key release nor a modifier key release while the dead
      // key compose sequence is pending should be reported as consumed.
      ui::KeyEvent dead_release = MakeX11KeyEvent(
          ui::EventType::kKeyReleased, ui::VKEY_OEM_4,
          ui::DomCode::BRACKET_LEFT,
          ui::DomKey::DeadKeyFromCombiningCharacter(kCombiningAcute),
          kUnusedKeyCode);
      EXPECT_FALSE(context.DispatchKeyEvent(dead_release));

      ui::KeyEvent shift_release = MakeX11KeyEvent(
          ui::EventType::kKeyReleased, ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
          ui::DomKey::SHIFT, kShiftLeftKeyCode);
      EXPECT_FALSE(context.DispatchKeyEvent(shift_release));

      // Subsequent base character still completes the dead-key composition (á).
      ui::KeyEvent a_press = MakeX11KeyEvent(
          ui::EventType::kKeyPressed, ui::VKEY_A, ui::DomCode::US_A,
          ui::DomKey::FromCharacter('a'), kKeyACode);
      EXPECT_TRUE(context.DispatchKeyEvent(a_press));
      EXPECT_EQ(delegate.committed_text(), u"\u00e1");

      ui::KeyEvent a_release = MakeX11KeyEvent(
          ui::EventType::kKeyReleased, ui::VKEY_A, ui::DomCode::US_A,
          ui::DomKey::FromCharacter('a'), kKeyACode);
      EXPECT_FALSE(context.DispatchKeyEvent(a_release));
    }
  }
}

TEST(InputMethodContextImplGtkTest,
     KeyReleaseRespectsAsyncImeHandledStateFlag) {
  base::ScopedEnvironmentVariableOverride im_override("GTK_IM_MODULE",
                                                      "gtk-im-context-simple");
  base::ScopedClosureRunner restore_keymap = MapDeadAcuteKeyCode();

  GtkUiPlatformStub platform;
  TestInputMethodContextDelegate delegate;
  InputMethodContextImplGtk context(&delegate, &platform);

  ui::LinuxInputMethodContext::TextInputClientAttributes attrs;
  attrs.input_type = ui::TEXT_INPUT_TYPE_TEXT;
  context.UpdateFocus(/*has_client=*/true, ui::TEXT_INPUT_TYPE_NONE, attrs,
                      ui::TextInputClient::FOCUS_REASON_OTHER);

  ui::KeyEvent dead_press = MakeX11KeyEvent(
      ui::EventType::kKeyPressed, ui::VKEY_OEM_4, ui::DomCode::BRACKET_LEFT,
      ui::DomKey::DeadKeyFromCombiningCharacter(kCombiningAcute),
      kUnusedKeyCode);
  EXPECT_TRUE(context.DispatchKeyEvent(dead_press));

  // Fallback release events re-injected with kPropertyKeyboardImeIgnoredFlag
  // must not be swallowed by GtkIMContextSimple.
  constexpr uint32_t kIgnoredState = ui::kPropertyKeyboardImeIgnoredFlag
                                     << ui::kPropertyKeyboardImeFlagOffset;
  ui::KeyEvent ignored_release = MakeX11KeyEvent(
      ui::EventType::kKeyReleased, ui::VKEY_OEM_4, ui::DomCode::BRACKET_LEFT,
      ui::DomKey::DeadKeyFromCombiningCharacter(kCombiningAcute),
      kUnusedKeyCode, kIgnoredState);
  EXPECT_FALSE(context.DispatchKeyEvent(ignored_release));

  // Async IME release events marked with kPropertyKeyboardImeHandledFlag
  // remain filtered while awaiting the fallback event.
  constexpr uint32_t kHandledState = ui::kPropertyKeyboardImeHandledFlag
                                     << ui::kPropertyKeyboardImeFlagOffset;
  ui::KeyEvent handled_release = MakeX11KeyEvent(
      ui::EventType::kKeyReleased, ui::VKEY_OEM_4, ui::DomCode::BRACKET_LEFT,
      ui::DomKey::DeadKeyFromCombiningCharacter(kCombiningAcute),
      kUnusedKeyCode, kHandledState);
  EXPECT_TRUE(context.DispatchKeyEvent(handled_release));
}

}  // namespace gtk
