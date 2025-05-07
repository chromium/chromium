// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_key_bindings_handler.h"

#include <array>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ui/base/glib/gsettings.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/events/event_constants.h"
#include "ui/gtk/gtk_compat.h"

namespace gtk {

namespace {

using KeyWithMods = std::pair<ui::KeyboardCode, ui::EventFlags>;

constexpr char kDesktopInterface[] = "org.gnome.desktop.interface";
constexpr char kGtkKeyTheme[] = "gtk-key-theme";
constexpr char kEmacsKeyTheme[] = "Emacs";

// This should contain at least all of the bindings in gtk/gtk-keys.css.emacs.
constexpr auto kEmacsBindings =
    base::MakeFixedFlatMap<KeyWithMods, ui::TextEditCommand>({
        {{ui::KeyboardCode::VKEY_BACK, ui::EF_ALT_DOWN},
         ui::TextEditCommand::DELETE_WORD_BACKWARD},
        {{ui::KeyboardCode::VKEY_A, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE},
        {{ui::KeyboardCode::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
         ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_B, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::MOVE_BACKWARD},
        {{ui::KeyboardCode::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
         ui::TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_B, ui::EF_ALT_DOWN},
         ui::TextEditCommand::MOVE_WORD_LEFT},
        {{ui::KeyboardCode::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
         ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_D, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::DELETE_FORWARD},
        {{ui::KeyboardCode::VKEY_D, ui::EF_ALT_DOWN},
         ui::TextEditCommand::DELETE_WORD_FORWARD},
        {{ui::KeyboardCode::VKEY_E, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::MOVE_TO_END_OF_LINE},
        {{ui::KeyboardCode::VKEY_E, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
         ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_F, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::MOVE_FORWARD},
        {{ui::KeyboardCode::VKEY_F, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
         ui::TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_F, ui::EF_ALT_DOWN},
         ui::TextEditCommand::MOVE_WORD_RIGHT},
        {{ui::KeyboardCode::VKEY_F, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
         ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_H, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::DELETE_BACKWARD},
        {{ui::KeyboardCode::VKEY_K, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::DELETE_TO_END_OF_LINE},
        {{ui::KeyboardCode::VKEY_N, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::MOVE_DOWN},
        {{ui::KeyboardCode::VKEY_N, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
         ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_P, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::MOVE_UP},
        {{ui::KeyboardCode::VKEY_P, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
         ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION},
        {{ui::KeyboardCode::VKEY_U, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE},
        {{ui::KeyboardCode::VKEY_W, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::DELETE_WORD_BACKWARD},
        {{ui::KeyboardCode::VKEY_Y, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::YANK},
        {{ui::KeyboardCode::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::UNDO},
        {{ui::KeyboardCode::VKEY_OEM_2, ui::EF_CONTROL_DOWN},
         ui::TextEditCommand::UNDO},
    });

}  // namespace

GtkKeyBindingsHandler::GtkKeyBindingsHandler() {
  settings_ = ui::GSettingsNew(kDesktopInterface);
  if (!settings_) {
    return;
  }
  signal_ = ScopedGSignal(
      settings_, "changed",
      base::BindRepeating(&GtkKeyBindingsHandler::OnSettingsChanged,
                          base::Unretained(this)));
  OnSettingsChanged(settings_.get(), kGtkKeyTheme);
}

GtkKeyBindingsHandler::~GtkKeyBindingsHandler() = default;

ui::TextEditCommand GtkKeyBindingsHandler::MatchEvent(const ui::Event& event) {
  if (!emacs_theme_) {
    return ui::TextEditCommand::INVALID_COMMAND;
  }
  auto* key_event = event.AsKeyEvent();
  if (!key_event) {
    return ui::TextEditCommand::INVALID_COMMAND;
  }
  if (key_event->is_char()) {
    return ui::TextEditCommand::INVALID_COMMAND;
  }

  constexpr auto kModMask = ui::EF_ALTGR_DOWN | ui::EF_ALT_DOWN |
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                            ui::EF_FUNCTION_DOWN | ui::EF_MOD3_DOWN |
                            ui::EF_SHIFT_DOWN;
  KeyWithMods key = {key_event->key_code(), key_event->flags() & kModMask};
  auto it = kEmacsBindings.find(key);
  return it == kEmacsBindings.end() ? ui::TextEditCommand::INVALID_COMMAND
                                    : it->second;
}

void GtkKeyBindingsHandler::OnSettingsChanged(GSettings* settings,
                                              const char* key) {
  DCHECK(settings);
  if (UNSAFE_TODO(strcmp(key, kGtkKeyTheme)) != 0) {
    return;
  }
  auto g_free_deleter = [](gchar* s) { g_free(s); };
  std::unique_ptr<gchar, decltype(g_free_deleter)> key_theme(
      g_settings_get_string(settings, kGtkKeyTheme), g_free_deleter);
  if (!key_theme) {
    return;
  }
  emacs_theme_ = UNSAFE_TODO(strcmp(key_theme.get(), kEmacsKeyTheme)) == 0;
}

}  // namespace gtk
