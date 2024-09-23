// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_KEY_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_KEY_H_

#include <stdint.h>

#include <optional>
#include <ostream>

#include "base/check.h"
#include "build/build_config.h"

namespace ui {

// Integer representation of UI Events KeyboardEvent.key value.
//
// The semantics follow the web string form[1]: the value is either a
// Unicode character or one of a defined set of additional values[2].
// There is one notable difference from the UI Events string key: for
// the 'Dead' key, this type provides a whole range of values that also
// encode the associated combining character. (They are not quite the
// same thing: a dead key is a non-printing operator that modifies a
// subsequent printing character, whereas a Unicode combining character
// is a printable character in its own right that attaches to a preceding
// character in a string.) This allows the interpretation of any keystroke
// to be carried as a single integer value.
//
// DomKey::NONE is a sentinel used to indicate an error or undefined value.
// It is not the same as Unicode code point 0 (ASCII NUL) or the valid DOM
// key 'Unidentified'.
//
// References:
// [1] http://www.w3.org/TR/uievents/#widl-KeyboardEvent-key
// [2] http://www.w3.org/TR/DOM-Level-3-Events-key/
//
class DomKey {
 public:
  using Base = uint32_t;

 private:
  // Integer representation of DomKey. This is arranged so that DomKey encoded
  // values are distinct from Unicode code points, so that we can dynamically
  // verify that they are not accidentally conflated.
  //
  // 31             24              16              8               0
  // |       |       |       |       |       |       |       |       |
  // | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
  // |        z        |c|s|                    v                    |
  //
  //  From low to high:
  //  - |v| is a value whose interpretation depends on the kind of key:
  //    - for a Unicode value, it is the code point (0 <= v <= 0x10FFFF);
  //    - for a dead key, the code point of the associated combining character;
  //    - for others, an arbitrary distinct value.
  //  - |s| is set for a valid symbolic key (i.e. not a Unicode character).
  //  - |c| is set if |v| holds a code point (for either a Unicode character
  //        directly, or a dead-key combining character).
  //  - |z| is reserved and always zero.
  //
  //  As consequences of this representation,
  //  - all valid DomKey encodings have at least one of |c| or |s| set, so
  //    they can't be confused with raw Unicode characters (where both are 0).
  //  - integer 0 is not a valid encoding, and can be used for DomKey::NONE.
  //
  enum { VALUE_BITS = 21 };
  enum Type : Base {
    VALUE_MASK = (1L << VALUE_BITS) - 1,
    TF_SYMBOLIC = (1L << VALUE_BITS),
    TF_CODEPOINT = (1L << (VALUE_BITS + 1)),
    TYPE_MASK = TF_CODEPOINT | TF_SYMBOLIC,
    TYPE_UNICODE = TF_CODEPOINT,
    TYPE_NON_UNICODE = TF_SYMBOLIC,
    TYPE_DEAD = TF_CODEPOINT | TF_SYMBOLIC,
  };
  static_assert(TYPE_UNICODE != 0 && TYPE_NON_UNICODE != 0 && TYPE_DEAD != 0,
                "suspicious representation change");

 public:
  // Following block is a technique to add inlined constant with C++14
  // compatible way. These can be replaced with inline constexpr after
  // C++17 support.
  enum : Base { NONE = 0 };
// |dom_key_data.inc| describes the non-printable DomKey values, and is
// included here to create constants for them in the DomKey:: scope.
#define DOM_KEY_MAP_DECLARATION_START enum Key : Base {
#define DOM_KEY_UNI(key, id, value) id = (TYPE_UNICODE | (value)),
#define DOM_KEY_MAP(key, id, value) id = (TYPE_NON_UNICODE | (value)),
#define DOM_KEY_MAP_DECLARATION_END \
  }                                 \
  ;
#include "ui/events/keycodes/dom/dom_key_data.inc"
#undef DOM_KEY_MAP_DECLARATION_START
#undef DOM_KEY_MAP
#undef DOM_KEY_UNI
#undef DOM_KEY_MAP_DECLARATION_END

  // Create a DomKey, with the undefined-value sentinel DomKey::NONE.
  constexpr DomKey() = default;

  // Create a DomKey from an encoded integer value. This is implicit so
  // that DomKey::NAME constants don't need to be explicitly converted
  // to DomKey.
  // After switching to C++17, this can be replaced by inline constexpr,
  // so can be private. On runtime, FromBase is preferred.
  constexpr DomKey(Base value) : value_(value) {}

  // Factory that returns a DomKey for the specified value. Returns nullopt if
  // |value| is not a valid value (or NONE).
  static std::optional<DomKey> FromBase(Base value) {
    if (value != 0 && !IsValidValue(value))
      return std::nullopt;
    return Base(value);
  }

  // Obtain the encoded integer representation of the DomKey.
  constexpr operator Base() const { return value_; }

  // True if the value is a valid DomKey (which excludes DomKey::NONE and
  // integers not following the DomKey format).
  bool IsValid() const { return IsValidValue(value_); }

  // True if the value is a Unicode code point.
  bool IsCharacter() const { return (value_ & TYPE_MASK) == TYPE_UNICODE; }

  // True if the value is a dead key.
  bool IsDeadKey() const { return (value_ & TYPE_MASK) == TYPE_DEAD; }

  // True if the value is the same as the value of DomKey::COMPOSE.
  bool IsComposeKey() const { return *this == DomKey::COMPOSE; }

  // Returns the Unicode code point for a Unicode key.
  // It is incorrect to call this for other kinds of key.
  uint32_t ToCharacter() const {
    DCHECK(IsCharacter()) << value_;
    return value_ & VALUE_MASK;
  }

  // Returns the associated combining code point for a dead key.
  // It is incorrect to call this for other kinds of key.
  uint32_t ToDeadKeyCombiningCharacter() const {
    DCHECK(IsDeadKey()) << value_;
    return value_ & VALUE_MASK;
  }

  // Returns a DomKey for the given Unicode character.
  constexpr static DomKey FromCharacter(uint32_t character) {
    DCHECK(character <= 0x10FFFF);
    return DomKey(TYPE_UNICODE | character);
  }

  // Returns a dead-key DomKey for the given combining character.
  constexpr static DomKey DeadKeyFromCombiningCharacter(
      uint32_t combining_character) {
    DCHECK(combining_character <= 0x10FFFF);
    return DomKey(TYPE_DEAD | combining_character);
  }

 private:
  static bool IsValidValue(Base value) { return (value & TYPE_MASK) != 0; }

  Base value_ = NONE;
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_KEY_H_
