// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHARACTER_COMPOSER_H_
#define UI_BASE_IME_CHARACTER_COMPOSER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace ui {
class KeyEvent;

// A class to recognize compose and dead key sequence.
// Outputs composed character.
class COMPONENT_EXPORT(UI_BASE_IME_TYPES) CharacterComposer {
 public:
  using ComposeBuffer = std::vector<DomKey>;

  // The U+00B7 "middle dot" character is also used by GTK to represent the
  // compose key in preedit strings.
  static constexpr char16_t kPreeditStringComposeKeySymbol = 0x00B7;

  // Decides in which modes the preedit string is enabled. If disabled for a
  // mode, it is always empty in that mode.
  enum class PreeditStringMode {
    // Enable in both hex and sequence mode.
    kAlwaysEnabled,
    // Enable in hex mode, disable in sequence mode.
    kHexModeOnly,
  };

  // Disable the preedit string in sequence mode by default.
  explicit CharacterComposer(
      PreeditStringMode mode = PreeditStringMode::kHexModeOnly);

  CharacterComposer(const CharacterComposer&) = delete;
  CharacterComposer& operator=(const CharacterComposer&) = delete;

  ~CharacterComposer();

  void Reset();

  // Filters keypress.
  // Returns true if the keypress is recognized as a part of composition
  // sequence.
  // Fabricated events which don't have the native event, are not supported.
  bool FilterKeyPress(const ui::KeyEvent& event);

  // Returns a string consisting of composed character.
  // Empty string is returned when there is no composition result.
  const std::u16string& composed_character() const {
    return composed_character_;
  }

  // Returns the preedit string.
  const std::u16string& preedit_string() const { return preedit_string_; }

 private:
  // An enum to describe composition mode.
  enum CompositionMode {
    // This is the initial state.
    // Composite a character with dead-keys and compose-key.
    KEY_SEQUENCE_MODE,
    // Composite a character with a hexadecimal unicode sequence.
    HEX_MODE,
  };

  // Filters keypress in key sequence mode.
  bool FilterKeyPressSequenceMode(const ui::KeyEvent& event);

  // Updates preedit string in key sequence mode.
  void UpdatePreeditStringSequenceMode();

  // Filters keypress in hexadecimal mode.
  bool FilterKeyPressHexMode(const ui::KeyEvent& event);

  // Commit a character composed from hexadecimal uncode sequence
  void CommitHex();

  // Updates preedit string in hexadecimal mode.
  void UpdatePreeditStringHexMode();

  // Remembers keypresses previously filtered.
  std::vector<DomKey> compose_buffer_;

  // Records hexadecimal digits previously filtered.
  std::vector<unsigned int> hex_buffer_;

  // A string representing the composed character.
  std::u16string composed_character_;

  // Preedit string.
  std::u16string preedit_string_;

  // Composition mode which this instance is in.
  CompositionMode composition_mode_ = KEY_SEQUENCE_MODE;

  const PreeditStringMode preedit_string_mode_;
};

// Abstract class for determining whether a ComposeBuffer forms a valid
// character composition sequence.
class ComposeChecker {
 public:
  enum class CheckSequenceResult {
    // The sequence is not a composition sequence or the prefix of any
    // composition sequence.
    NO_MATCH,
    // The sequence is a prefix of one or more composition sequences.
    PREFIX_MATCH,
    // The sequence matches a composition sequence.
    FULL_MATCH
  };
  ComposeChecker() = default;

  ComposeChecker(const ComposeChecker&) = delete;
  ComposeChecker& operator=(const ComposeChecker&) = delete;

  virtual ~ComposeChecker() {}
  virtual CheckSequenceResult CheckSequence(
      const ui::CharacterComposer::ComposeBuffer& sequence,
      uint32_t* composed_character) const = 0;
};

// Implementation of |ComposeChecker| using a compact generated tree.
class TreeComposeChecker : public ComposeChecker {
 public:
  struct CompositionData {
    size_t maximum_sequence_length;
    int tree_entries;
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #reinterpret-cast-trivial-type, #global-scope
    RAW_PTR_EXCLUSION const uint16_t* tree;
  };

  explicit TreeComposeChecker(const CompositionData& data) : data_(data) {}
  CheckSequenceResult CheckSequence(
      const ui::CharacterComposer::ComposeBuffer& sequence,
      uint32_t* composed_character) const override;

 private:
  bool Find(uint16_t index, uint16_t size, uint16_t key, uint16_t* value) const;
  const raw_ref<const CompositionData> data_;
};

}  // namespace ui

#endif  // UI_BASE_IME_CHARACTER_COMPOSER_H_
