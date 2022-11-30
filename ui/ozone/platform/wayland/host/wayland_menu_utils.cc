// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_menu_utils.h"

#include "base/strings/utf_string_conversion_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

namespace ui {

WaylandMenuUtils::WaylandMenuUtils(WaylandConnection* connection)
    : connection_(connection) {}

WaylandMenuUtils::~WaylandMenuUtils() = default;

int WaylandMenuUtils::GetCurrentKeyModifiers() const {
  DCHECK(connection_);
  DCHECK(connection_->event_source());
  return connection_->event_source()->keyboard_modifiers();
}

std::string WaylandMenuUtils::ToDBusKeySym(KeyboardCode code) const {
  // KeyboardCode is VKEY_***, and here we convert it into the string
  // representation.
  DomCode dom_code = UsLayoutKeyboardCodeToDomCode(code);

  KeyboardLayoutEngine* keyboard_layout_engine =
      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  std::unique_ptr<StubKeyboardLayoutEngine> stub_layout_engine;
  if (!keyboard_layout_engine) {
    stub_layout_engine = std::make_unique<StubKeyboardLayoutEngine>();
    keyboard_layout_engine = stub_layout_engine.get();
  }

  DomKey dom_key;
  KeyboardCode key_code_ignored;
  if (!keyboard_layout_engine->Lookup(dom_code, EF_NONE, &dom_key,
                                      &key_code_ignored) ||
      !dom_key.IsCharacter()) {
    // The keycode lookup failed, or mapped to a key that isn't a unicode
    // character.  Return an empty string.
    return {};
  }

  std::string result;
  base::WriteUnicodeCharacter(dom_key.ToCharacter(), &result);
  return result;
}

}  // namespace ui
