// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_key_bindings_handler.h"

#include <array>
#include <memory>
#include <utility>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

#if defined(USE_GSETTINGS)
#define G_SETTINGS_ENABLE_BACKEND 1
#include <gio/gio.h>
#include <gio/gsettingsbackend.h>
#endif

namespace ui {

namespace {

KeyEvent MakeKeyPress(KeyboardCode key_code, int flags) {
  return KeyEvent(EventType::kKeyPressed, key_code, flags);
}

#if defined(USE_GSETTINGS)
constexpr char kDesktopInterface[] = "org.gnome.desktop.interface";
constexpr char kGtkKeyTheme[] = "gtk-key-theme";

ScopedGObject<GSettings> CreateMemorySettings(const char* schema_id) {
  GSettingsSchemaSource* source = g_settings_schema_source_get_default();
  if (!source) {
    return {};
  }
  std::unique_ptr<GSettingsSchema, decltype(&g_settings_schema_unref)> schema(
      g_settings_schema_source_lookup(source, schema_id, /*recursive=*/true),
      &g_settings_schema_unref);
  if (!schema) {
    return {};
  }
  ScopedGObject<GSettingsBackend> backend =
      TakeGObject(g_memory_settings_backend_new());
  return TakeGObject(
      g_settings_new_full(schema.get(), backend.get(), /*path=*/nullptr));
}
#endif

}  // namespace

TEST(LinuxKeyBindingsHandlerTest, DefaultConstructorSmoke) {
  LinuxKeyBindingsHandler handler;
  MouseEvent mouse_event(EventType::kMousePressed, gfx::Point(), gfx::Point(),
                         base::TimeTicks(), EF_NONE, EF_NONE);
  EXPECT_EQ(handler.MatchEvent(mouse_event), TextEditCommand::INVALID_COMMAND);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_LEFT, EF_NONE)),
            TextEditCommand::INVALID_COMMAND);
#if !defined(USE_GSETTINGS)
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);
#endif
}

#if defined(USE_GSETTINGS)
TEST(LinuxKeyBindingsHandlerTest, DefaultThemeReturnsInvalidCommand) {
  ScopedGObject<GSettings> settings = CreateMemorySettings(kDesktopInterface);
  if (!settings) {
    GTEST_SKIP() << kDesktopInterface << " GSettings schema is not installed.";
  }
  ASSERT_TRUE(g_settings_set_string(settings.get(), kGtkKeyTheme, "Default"));

  LinuxKeyBindingsHandler handler(std::move(settings));
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_E, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_K, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);
}

TEST(LinuxKeyBindingsHandlerTest, EmacsThemeBindings) {
  ScopedGObject<GSettings> settings = CreateMemorySettings(kDesktopInterface);
  if (!settings) {
    GTEST_SKIP() << kDesktopInterface << " GSettings schema is not installed.";
  }
  ASSERT_TRUE(g_settings_set_string(settings.get(), kGtkKeyTheme, "Emacs"));

  LinuxKeyBindingsHandler handler(std::move(settings));

  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_BACK, EF_ALT_DOWN)),
            TextEditCommand::DELETE_WORD_BACKWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_TO_BEGINNING_OF_LINE);
  EXPECT_EQ(
      handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN | EF_SHIFT_DOWN)),
      TextEditCommand::MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_B, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_BACKWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_B, EF_ALT_DOWN)),
            TextEditCommand::MOVE_WORD_LEFT);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_D, EF_CONTROL_DOWN)),
            TextEditCommand::DELETE_FORWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_D, EF_ALT_DOWN)),
            TextEditCommand::DELETE_WORD_FORWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_E, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_TO_END_OF_LINE);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_F, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_FORWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_F, EF_ALT_DOWN)),
            TextEditCommand::MOVE_WORD_RIGHT);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_H, EF_CONTROL_DOWN)),
            TextEditCommand::DELETE_BACKWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_K, EF_CONTROL_DOWN)),
            TextEditCommand::DELETE_TO_END_OF_LINE);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_N, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_DOWN);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_P, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_UP);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_U, EF_CONTROL_DOWN)),
            TextEditCommand::DELETE_TO_BEGINNING_OF_LINE);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_W, EF_CONTROL_DOWN)),
            TextEditCommand::DELETE_WORD_BACKWARD);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_Y, EF_CONTROL_DOWN)),
            TextEditCommand::YANK);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_OEM_MINUS, EF_CONTROL_DOWN)),
            TextEditCommand::UNDO);
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_OEM_2, EF_CONTROL_DOWN)),
            TextEditCommand::UNDO);

  // Lock modifiers are ignored when matching bindings.
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(
                VKEY_A, EF_CONTROL_DOWN | EF_CAPS_LOCK_ON | EF_NUM_LOCK_ON)),
            TextEditCommand::MOVE_TO_BEGINNING_OF_LINE);

  // Extra modifiers that are not part of the binding should not match.
  EXPECT_EQ(
      handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN | EF_ALT_DOWN)),
      TextEditCommand::INVALID_COMMAND);

  // Character events should not match key bindings.
  KeyEvent char_event =
      KeyEvent::FromCharacter('a', VKEY_A, DomCode::US_A, EF_CONTROL_DOWN);
  EXPECT_EQ(handler.MatchEvent(char_event), TextEditCommand::INVALID_COMMAND);

  // Arrow keys have no Emacs binding, so they are left to the text field
  // (e.g. Blink handles them in vertical text fields; see crbug.com/484651).
  for (KeyboardCode code : {VKEY_LEFT, VKEY_RIGHT, VKEY_UP, VKEY_DOWN}) {
    EXPECT_EQ(handler.MatchEvent(MakeKeyPress(code, EF_NONE)),
              TextEditCommand::INVALID_COMMAND);
  }
}

TEST(LinuxKeyBindingsHandlerTest, DynamicThemeChangeSignal) {
  ScopedGObject<GSettings> settings = CreateMemorySettings(kDesktopInterface);
  if (!settings) {
    GTEST_SKIP() << kDesktopInterface << " GSettings schema is not installed.";
  }
  GSettings* raw_settings = settings.get();
  ASSERT_TRUE(g_settings_set_string(raw_settings, kGtkKeyTheme, "Default"));

  LinuxKeyBindingsHandler handler(std::move(settings));
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);

  ASSERT_TRUE(g_settings_set_string(raw_settings, kGtkKeyTheme, "Emacs"));
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::MOVE_TO_BEGINNING_OF_LINE);

  ASSERT_TRUE(g_settings_set_string(raw_settings, kGtkKeyTheme, "Default"));
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);
}

TEST(LinuxKeyBindingsHandlerTest, SchemaMissingKeyHandledGracefully) {
  LinuxKeyBindingsHandler null_handler{ScopedGObject<GSettings>()};
  EXPECT_EQ(null_handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);

  constexpr std::array kSchemasWithoutKeyTheme = {
      "org.gnome.settings-daemon.plugins.xsettings",
      "org.gtk.Settings.FileChooser",
      "org.gnome.desktop.wm.preferences",
  };
  ScopedGObject<GSettings> other_settings;
  for (const char* schema_id : kSchemasWithoutKeyTheme) {
    other_settings = CreateMemorySettings(schema_id);
    if (other_settings) {
      break;
    }
  }
  if (!other_settings) {
    GTEST_SKIP() << "Could not find a test schema without gtk-key-theme.";
  }

  LinuxKeyBindingsHandler handler(std::move(other_settings));
  EXPECT_EQ(handler.MatchEvent(MakeKeyPress(VKEY_A, EF_CONTROL_DOWN)),
            TextEditCommand::INVALID_COMMAND);
}
#endif  // defined(USE_GSETTINGS)

}  // namespace ui
