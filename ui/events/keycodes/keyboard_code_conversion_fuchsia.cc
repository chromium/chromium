// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_fuchsia.h"

#include <fuchsia/ui/input3/cpp/fidl.h>

#include "base/containers/flat_map.h"

namespace ui {
namespace {

DomKey DomKeyFromFuchsiaNonPrintableKey(
    const fuchsia::ui::input3::NonPrintableKey& key) {
  if (key == fuchsia::ui::input3::NonPrintableKey::ENTER)
    return DomKey::ENTER;
  if (key == fuchsia::ui::input3::NonPrintableKey::TAB)
    return DomKey::TAB;
  if (key == fuchsia::ui::input3::NonPrintableKey::BACKSPACE)
    return DomKey::BACKSPACE;

  return DomKey::UNIDENTIFIED;
}

}  // namespace

DomKey DomKeyFromFuchsiaKeyMeaning(
    const fuchsia::ui::input3::KeyMeaning& key_meaning) {
  if (key_meaning.is_codepoint()) {
    // TODO(fxbug.dev/106600): Remove this check for codepoint zero, once the
    // platform provides non-printable key meanings consistently.
    if (key_meaning.codepoint() == 0)
      return DomKey::UNIDENTIFIED;

    return DomKey::FromCharacter(key_meaning.codepoint());
  }

  if (key_meaning.is_non_printable_key())
    return DomKeyFromFuchsiaNonPrintableKey(key_meaning.non_printable_key());

  return DomKey::UNIDENTIFIED;
}

}  // namespace ui
