// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

enum class DomCode : uint32_t;

// Class for a single keyboard layout (if there is only one group) or single
// layout group within a keyboard layout.
//
// Note that a "group" is not a group of layouts but is rather a "sub-group"
// of a layout. Most layouts have a single group, but layouts can be divided
// into multiple groups. These groups are effectively separate sub-layouts that
// can be enabled within the layout. For example, a Japanese layout can have a
// main group and a separate "kana" group.
class DomKeyboardLayout final {
 public:
  DomKeyboardLayout();

  DomKeyboardLayout(const DomKeyboardLayout&) = delete;
  DomKeyboardLayout& operator=(const DomKeyboardLayout&) = delete;

  ~DomKeyboardLayout();

  // Add a DomCode -> Unicode mapping for this layout (or layout group).
  // Only unshifted (shift level = 0) values should be recorded.
  void AddKeyMapping(DomCode code, uint32_t unicode);

  // Return a dom code string -> dom key string mapping table.
  base::flat_map<std::string, std::string> GetMap();

  // Return true if this layout can generate all the lowercase ASCII
  // characters using only unshifted key presses.
  bool IsAsciiCapable();

 private:
  // Mapping from DomCode -> Unicode character.
  base::flat_map<ui::DomCode, uint32_t> layout_;
};

// An array of DomCodes that identifies the Writing System Keys on the
// keyboard.
//
// The Writing System Keys are those that change meaning (i.e., they produce
// a different KeyboardEvent key value) based on the current keyboard layout.
// See https://www.w3.org/TR/uievents-code/#key-alphanumeric-writing-system
//
// This is used by the Keyboard Map API
// (see https://wicg.github.io/keyboard-map/)
extern const DomCode writing_system_key_domcodes[];

extern const size_t kWritingSystemKeyDomCodeEntries;

extern const uint32_t kHankakuZenkakuPlaceholder;

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_H_
