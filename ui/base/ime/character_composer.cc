// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/ime/character_composer.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

#include "ui/base/ime/character_composer_data.h"

bool CheckCharacterComposeTable(
    const ui::CharacterComposer::ComposeBuffer& compose_sequence,
    uint32_t* composed_character) {
  const ui::TreeComposeChecker kTreeComposeChecker(kCompositions);
  return kTreeComposeChecker.CheckSequence(compose_sequence,
                                           composed_character) !=
         ui::ComposeChecker::CheckSequenceResult::NO_MATCH;
}

// Converts |character| to UTF16 string.
// Returns false when |character| is not a valid character.
bool UTF32CharacterToUTF16(uint32_t character, std::u16string* output) {
  output->clear();
  // Reject invalid character. (e.g. codepoint greater than 0x10ffff)
  if (!CBU_IS_UNICODE_CHAR(character))
    return false;
  if (character) {
    output->resize(CBU16_LENGTH(character));
    size_t i = 0;
    CBU16_APPEND_UNSAFE(&(*output)[0], i, character);
  }
  return true;
}

// Returns an hexadecimal digit integer (0 to 15) corresponding to |keycode|.
// -1 is returned when |keycode| cannot be a hexadecimal digit.
int KeycodeToHexDigit(unsigned int keycode) {
  if (ui::VKEY_0 <= keycode && keycode <= ui::VKEY_9)
    return keycode - ui::VKEY_0;
  if (ui::VKEY_A <= keycode && keycode <= ui::VKEY_F)
    return keycode - ui::VKEY_A + 10;
  return -1;  // |keycode| cannot be a hexadecimal digit.
}

// `ui::DomKey` only offers `ToDeadKeyCombiningCharacter()`, but we need the
// non-combining character for the dead key for the preedit string. If we use
// the combining character, it may combine with the character preceding the
// preedit string, which is unwanted and confusing.
std::optional<char16_t> DeadKeyToNonCombiningCharacter(ui::DomKey dom_key) {
  CHECK(dom_key.IsDeadKey());
  uint32_t combining_char = dom_key.ToDeadKeyCombiningCharacter();

  // Unicode's list of "Combining Diacritical Marks"
  // (https://www.unicode.org/charts/PDF/U0300.pdf) is much longer, but these
  // should be the most commonly used ones.
  switch (combining_char) {
    // Combining grave.
    case 0x300:
      return u'`';
    // Combining acute.
    case 0x301:
      return u'´';
    // Combining circumflex.
    case 0x302:
      return u'^';
    // Combining tilde.
    case 0x303:
      return u'~';
    // Combining diaeresis.
    case 0x308:
      return u'¨';
    // Unknown combining character.
    default:
      LOG(WARNING) << "Unable to convert unknown dead key combining character "
                      "to non-combining variant: U+"
                   << base::StringPrintf("%04d", combining_char);
      return std::nullopt;
  }
}

}  // namespace

namespace ui {

CharacterComposer::CharacterComposer(PreeditStringMode mode)
    : preedit_string_mode_(mode) {}

CharacterComposer::~CharacterComposer() = default;

void CharacterComposer::Reset() {
  compose_buffer_.clear();
  hex_buffer_.clear();
  composed_character_.clear();
  preedit_string_.clear();
  composition_mode_ = KEY_SEQUENCE_MODE;
}

bool CharacterComposer::FilterKeyPress(const ui::KeyEvent& event) {
  if (event.type() != EventType::kKeyPressed &&
      event.type() != EventType::kKeyReleased) {
    return false;
  }

  // We don't care about modifier key presses.
  if (KeycodeConverter::IsDomKeyForModifier(event.GetDomKey()))
    return false;

  composed_character_.clear();
  preedit_string_.clear();

  // When the user presses Ctrl+Shift+U, maybe switch to HEX_MODE.
  // We don't care about other modifiers like Alt.  When CapsLock is on, we do
  // nothing because what we receive is Ctrl+Shift+u (not U).
  if (event.key_code() == VKEY_U &&
      (event.flags() & (EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_CAPS_LOCK_ON)) ==
          (EF_SHIFT_DOWN | EF_CONTROL_DOWN)) {
    if (composition_mode_ == KEY_SEQUENCE_MODE && compose_buffer_.empty()) {
      // There is no ongoing composition.  Let's switch to HEX_MODE.
      composition_mode_ = HEX_MODE;
      UpdatePreeditStringHexMode();
      return true;
    }
  }

  // Filter key press in an appropriate manner.
  switch (composition_mode_) {
    case KEY_SEQUENCE_MODE:
      return FilterKeyPressSequenceMode(event);
    case HEX_MODE:
      return FilterKeyPressHexMode(event);
    default:
      NOTREACHED();
  }
}

bool CharacterComposer::FilterKeyPressSequenceMode(const KeyEvent& event) {
  DCHECK(composition_mode_ == KEY_SEQUENCE_MODE);
  compose_buffer_.push_back(event.GetDomKey());

  // Check compose table.
  uint32_t composed_character_utf32 = 0;
  if (CheckCharacterComposeTable(compose_buffer_, &composed_character_utf32)) {
    // Key press is recognized as a part of composition.
    if (composed_character_utf32 != 0) {
      // We get a composed character.
      compose_buffer_.clear();
      UTF32CharacterToUTF16(composed_character_utf32, &composed_character_);
    }

    if (preedit_string_mode_ == PreeditStringMode::kAlwaysEnabled) {
      UpdatePreeditStringSequenceMode();
    }

    return true;
  }
  // Key press is not a part of composition.
  compose_buffer_.pop_back();  // Remove the keypress added this time.
  if (!compose_buffer_.empty()) {
    // Check for Windows-style composition fallback: If the dead key encodes
    // a printable ASCII character, output that followed by the new keypress.
    // (This could be extended to allow any printable Unicode character in
    // the dead key, and/or for longer sequences, but there is no current use
    // for that, so we keep it simple.)
    if ((compose_buffer_.size() == 1) && (compose_buffer_[0].IsDeadKey())) {
      int32_t dead_character = compose_buffer_[0].ToDeadKeyCombiningCharacter();
      if (dead_character >= 0x20 && dead_character <= 0x7E) {
        DomKey current_key = event.GetDomKey();
        int32_t current_character = 0;
        if (current_key.IsCharacter())
          current_character = current_key.ToCharacter();
        else if (current_key.IsDeadKey())
          current_character = current_key.ToDeadKeyCombiningCharacter();
        if (current_character) {
          base::WriteUnicodeCharacter(dead_character, &composed_character_);
          base::WriteUnicodeCharacter(current_character, &composed_character_);
        }
      }
    }
    compose_buffer_.clear();

    if (preedit_string_mode_ == PreeditStringMode::kAlwaysEnabled) {
      UpdatePreeditStringSequenceMode();
    }

    return true;
  }
  return false;
}

void CharacterComposer::UpdatePreeditStringSequenceMode() {
  CHECK_EQ(preedit_string_mode_, PreeditStringMode::kAlwaysEnabled);
  for (auto key : compose_buffer_) {
    if (key.IsCharacter()) {
      base::WriteUnicodeCharacter(key.ToCharacter(), &preedit_string_);
    } else if (key.IsDeadKey()) {
      if (std::optional<char16_t> non_combining_character =
              DeadKeyToNonCombiningCharacter(key)) {
        base::WriteUnicodeCharacter(*non_combining_character, &preedit_string_);
      }
    } else if (key.IsComposeKey() && (compose_buffer_.size() == 1)) {
      base::WriteUnicodeCharacter(kPreeditStringComposeKeySymbol,
                                  &preedit_string_);
    }
  }
}

bool CharacterComposer::FilterKeyPressHexMode(const KeyEvent& event) {
  DCHECK(composition_mode_ == HEX_MODE);
  const size_t kMaxHexSequenceLength = 8;
  char16_t c = event.GetCharacter();
  int hex_digit = 0;
  if (base::IsHexDigit(c)) {
    hex_digit = base::HexDigitToInt(c);
  } else {
    // With 101 keyboard, control + shift + 3 produces '#', but a user may
    // have intended to type '3'.  So, if a hexadecimal character was not found,
    // suppose a user is holding shift key (and possibly control key, too) and
    // try a character with modifier keys removed.
    hex_digit = KeycodeToHexDigit(event.key_code());
  }
  if (hex_digit >= 0) {
    if (hex_buffer_.size() < kMaxHexSequenceLength) {
      // Add the key to the buffer if it is a hex digit.
      hex_buffer_.push_back(hex_digit);
    }
  } else {
    DomKey key = event.GetDomKey();
    if (key == DomKey::ESCAPE) {
      // Cancel composition when ESC is pressed.
      Reset();
    } else if (key == DomKey::ENTER || c == ' ') {
      // Commit the composed character when Enter or space is pressed.
      CommitHex();
    } else if (key == DomKey::BACKSPACE) {
      // Pop back the buffer when Backspace is pressed.
      if (!hex_buffer_.empty()) {
        hex_buffer_.pop_back();
      } else {
        // If there is no character in |hex_buffer_|, cancel composition.
        Reset();
      }
    }
    // Other keystrokes are ignored in hex composition mode.
  }
  UpdatePreeditStringHexMode();
  return true;
}

void CharacterComposer::CommitHex() {
  DCHECK(composition_mode_ == HEX_MODE);
  uint32_t composed_character_utf32 = 0;
  for (size_t i = 0; i != hex_buffer_.size(); ++i) {
    const uint32_t digit = hex_buffer_[i];
    DCHECK(0 <= digit && digit < 16);
    composed_character_utf32 <<= 4;
    composed_character_utf32 |= digit;
  }
  Reset();
  UTF32CharacterToUTF16(composed_character_utf32, &composed_character_);
}

void CharacterComposer::UpdatePreeditStringHexMode() {
  if (composition_mode_ != HEX_MODE) {
    preedit_string_.clear();
    return;
  }
  std::string preedit_string_ascii("u");
  for (size_t i = 0; i != hex_buffer_.size(); ++i) {
    const int digit = hex_buffer_[i];
    DCHECK(0 <= digit && digit < 16);
    preedit_string_ascii += digit <= 9 ? ('0' + digit) : ('a' + (digit - 10));
  }
  preedit_string_ = base::ASCIIToUTF16(preedit_string_ascii);
}

ComposeChecker::CheckSequenceResult TreeComposeChecker::CheckSequence(
    const ui::CharacterComposer::ComposeBuffer& sequence,
    uint32_t* composed_character) const {
  *composed_character = 0;
  if (sequence.size() > data_->maximum_sequence_length)
    return CheckSequenceResult::NO_MATCH;

  uint16_t tree_index = 0;
  for (const auto& keystroke : sequence) {
    DCHECK(tree_index < data_->tree_entries);

    // If we are looking up a dead key or the Compose key, skip over the
    // character tables.
    int32_t character = -1;
    if (keystroke.IsDeadKey() || keystroke.IsComposeKey()) {
      tree_index += 2 * data_->tree[tree_index] + 1;  // internal unicode table
      tree_index += 2 * data_->tree[tree_index] + 1;  // leaf unicode table
      // The generate_character_composer_data.py script assigns 0 to the Compose
      // key.
      character = keystroke.IsComposeKey()
                      ? 0
                      : keystroke.ToDeadKeyCombiningCharacter();
    } else if (keystroke.IsCharacter()) {
      character = keystroke.ToCharacter();
    }
    if (character < 0 || character > 0xFFFF)
      return CheckSequenceResult::NO_MATCH;

    // Check the internal subtree table.
    uint16_t result = 0;
    uint16_t entries = data_->tree[tree_index++];
    if (entries &&
        Find(tree_index, entries, static_cast<uint16_t>(character), &result)) {
      tree_index = result;
      continue;
    }

    // Skip over the internal subtree table and check the leaf table.
    tree_index += 2 * entries;
    entries = data_->tree[tree_index++];
    if (entries &&
        Find(tree_index, entries, static_cast<uint16_t>(character), &result)) {
      *composed_character = result;
      return CheckSequenceResult::FULL_MATCH;
    }
    return CheckSequenceResult::NO_MATCH;
  }
  return CheckSequenceResult::PREFIX_MATCH;
}

bool TreeComposeChecker::Find(uint16_t index,
                              uint16_t size,
                              uint16_t key,
                              uint16_t* value) const {
  struct TableEntry {
    uint16_t key;
    uint16_t value;
    bool operator<(const TableEntry& other) const {
      return this->key < other.key;
    }
  };
  const TableEntry* a =
      reinterpret_cast<const TableEntry*>(&data_->tree[index]);
  const TableEntry* z = a + size;
  const TableEntry target = {key, 0};
  const TableEntry* it = std::lower_bound(a, z, target);
  if ((it != z) && (it->key == key)) {
    *value = it->value;
    return true;
  }
  return false;
}

}  // namespace ui
