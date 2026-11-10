// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_key_bindings_handler.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(USE_GSETTINGS)
#include <gio/gio.h>

#include "ui/base/glib/gsettings.h"
#endif

namespace ui {

namespace {

using KeyWithMods = std::pair<KeyboardCode, EventFlags>;

#if defined(USE_GSETTINGS)
constexpr char kDesktopInterface[] = "org.gnome.desktop.interface";
constexpr char kGtkKeyTheme[] = "gtk-key-theme";
constexpr char kEmacsKeyTheme[] = "Emacs";

bool HasValidStringKey(GSettings* settings, const char* key_name) {
  GSettingsSchema* raw_schema = nullptr;
  g_object_get(settings, "settings-schema", &raw_schema, nullptr);
  if (!raw_schema) {
    return false;
  }
  std::unique_ptr<GSettingsSchema, decltype(&g_settings_schema_unref)> schema(
      raw_schema, &g_settings_schema_unref);
  if (!g_settings_schema_has_key(schema.get(), key_name)) {
    return false;
  }
  std::unique_ptr<GSettingsSchemaKey, decltype(&g_settings_schema_key_unref)>
      key(g_settings_schema_get_key(schema.get(), key_name),
          &g_settings_schema_key_unref);
  return key &&
         g_variant_type_equal(g_settings_schema_key_get_value_type(key.get()),
                              G_VARIANT_TYPE_STRING);
}
#endif

// This should contain at least all of the bindings in gtk/gtk-keys.css.emacs.
constexpr auto kEmacsBindings = base::MakeFixedFlatMap<KeyWithMods,
                                                       TextEditCommand>({
    {{KeyboardCode::VKEY_BACK, EF_ALT_DOWN},
     TextEditCommand::DELETE_WORD_BACKWARD},
    {{KeyboardCode::VKEY_A, EF_CONTROL_DOWN},
     TextEditCommand::MOVE_TO_BEGINNING_OF_LINE},
    {{KeyboardCode::VKEY_A, EF_CONTROL_DOWN | EF_SHIFT_DOWN},
     TextEditCommand::MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_B, EF_CONTROL_DOWN}, TextEditCommand::MOVE_BACKWARD},
    {{KeyboardCode::VKEY_B, EF_CONTROL_DOWN | EF_SHIFT_DOWN},
     TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_B, EF_ALT_DOWN}, TextEditCommand::MOVE_WORD_LEFT},
    {{KeyboardCode::VKEY_B, EF_SHIFT_DOWN | EF_ALT_DOWN},
     TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_D, EF_CONTROL_DOWN}, TextEditCommand::DELETE_FORWARD},
    {{KeyboardCode::VKEY_D, EF_ALT_DOWN}, TextEditCommand::DELETE_WORD_FORWARD},
    {{KeyboardCode::VKEY_E, EF_CONTROL_DOWN},
     TextEditCommand::MOVE_TO_END_OF_LINE},
    {{KeyboardCode::VKEY_E, EF_CONTROL_DOWN | EF_SHIFT_DOWN},
     TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_F, EF_CONTROL_DOWN}, TextEditCommand::MOVE_FORWARD},
    {{KeyboardCode::VKEY_F, EF_CONTROL_DOWN | EF_SHIFT_DOWN},
     TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_F, EF_ALT_DOWN}, TextEditCommand::MOVE_WORD_RIGHT},
    {{KeyboardCode::VKEY_F, EF_SHIFT_DOWN | EF_ALT_DOWN},
     TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_H, EF_CONTROL_DOWN}, TextEditCommand::DELETE_BACKWARD},
    {{KeyboardCode::VKEY_K, EF_CONTROL_DOWN},
     TextEditCommand::DELETE_TO_END_OF_LINE},
    {{KeyboardCode::VKEY_N, EF_CONTROL_DOWN}, TextEditCommand::MOVE_DOWN},
    {{KeyboardCode::VKEY_N, EF_CONTROL_DOWN | EF_SHIFT_DOWN},
     TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_P, EF_CONTROL_DOWN}, TextEditCommand::MOVE_UP},
    {{KeyboardCode::VKEY_P, EF_CONTROL_DOWN | EF_SHIFT_DOWN},
     TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION},
    {{KeyboardCode::VKEY_U, EF_CONTROL_DOWN},
     TextEditCommand::DELETE_TO_BEGINNING_OF_LINE},
    {{KeyboardCode::VKEY_W, EF_CONTROL_DOWN},
     TextEditCommand::DELETE_WORD_BACKWARD},
    {{KeyboardCode::VKEY_Y, EF_CONTROL_DOWN}, TextEditCommand::YANK},
    {{KeyboardCode::VKEY_OEM_MINUS, EF_CONTROL_DOWN}, TextEditCommand::UNDO},
    {{KeyboardCode::VKEY_OEM_2, EF_CONTROL_DOWN}, TextEditCommand::UNDO},
});

}  // namespace

#if defined(USE_GSETTINGS)
LinuxKeyBindingsHandler::LinuxKeyBindingsHandler()
    : LinuxKeyBindingsHandler(GSettingsNew(kDesktopInterface)) {}

LinuxKeyBindingsHandler::LinuxKeyBindingsHandler(
    ScopedGObject<GSettings> settings)
    : settings_(std::move(settings)) {
  if (!settings_ || !HasValidStringKey(settings_.get(), kGtkKeyTheme)) {
    settings_.Reset();
    return;
  }
  signal_ = ScopedGSignal(
      settings_, "changed",
      base::BindRepeating(&LinuxKeyBindingsHandler::OnSettingsChanged,
                          weak_factory_.GetWeakPtr()));
  OnSettingsChanged(settings_.get(), kGtkKeyTheme);
}
#else
LinuxKeyBindingsHandler::LinuxKeyBindingsHandler() = default;
#endif

LinuxKeyBindingsHandler::~LinuxKeyBindingsHandler() = default;

TextEditCommand LinuxKeyBindingsHandler::MatchEvent(const Event& event) {
  if (!event.IsKeyEvent()) {
    return TextEditCommand::INVALID_COMMAND;
  }
  const KeyEvent* key_event = event.AsKeyEvent();

  if (!emacs_theme_ || key_event->is_char()) {
    return TextEditCommand::INVALID_COMMAND;
  }

  constexpr auto kModMask = EF_ALTGR_DOWN | EF_ALT_DOWN | EF_COMMAND_DOWN |
                            EF_CONTROL_DOWN | EF_FUNCTION_DOWN | EF_MOD3_DOWN |
                            EF_SHIFT_DOWN;
  KeyWithMods key = {key_event->key_code(), key_event->flags() & kModMask};
  auto it = kEmacsBindings.find(key);
  return it == kEmacsBindings.end() ? TextEditCommand::INVALID_COMMAND
                                    : it->second;
}

#if defined(USE_GSETTINGS)
void LinuxKeyBindingsHandler::OnSettingsChanged(GSettings* settings,
                                                const char* key) {
  CHECK(settings, base::NotFatalUntil::M163);
  if (!key || std::string_view(key) != kGtkKeyTheme) {
    return;
  }
  auto g_free_deleter = [](gchar* s) { g_free(s); };
  std::unique_ptr<gchar, decltype(g_free_deleter)> key_theme(
      g_settings_get_string(settings, kGtkKeyTheme), g_free_deleter);
  if (!key_theme) {
    return;
  }
  emacs_theme_ = (std::string_view(key_theme.get()) == kEmacsKeyTheme);
}
#endif

}  // namespace ui
