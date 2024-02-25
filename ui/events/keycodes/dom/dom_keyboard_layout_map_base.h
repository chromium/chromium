// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MAP_BASE_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MAP_BASE_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"

namespace ui {

enum class DomCode : uint32_t;
class DomKey;
class DomKeyboardLayout;

// Provides the platform agnostic logic for generating a dom keyboard layout
// map, subclassing is required for each platform to retrieve the layout
// information from the underlying operating system.
class DomKeyboardLayoutMapBase {
 public:
  DomKeyboardLayoutMapBase(const DomKeyboardLayoutMapBase&) = delete;
  DomKeyboardLayoutMapBase& operator=(const DomKeyboardLayoutMapBase&) = delete;

  virtual ~DomKeyboardLayoutMapBase();

  // Generates a KeyboardLayoutMap based on the keyboard layouts provided by the
  // operating system.
  base::flat_map<std::string, std::string> Generate();

 protected:
  DomKeyboardLayoutMapBase();

  // Returns the number of keyboard layouts available from the operating system.
  // It could represent the set of all layouts, if available, or only the active
  // layout, depending on what the platform provides.
  virtual uint32_t GetKeyboardLayoutCount() = 0;

  // Returns an initialized DomKey using the value of |dom_code| associated with
  // |keyboard_layout_index| using platform APIs.  |keyboard_layout_index| is a
  // value in the interval of [0, keyboard_layout_count) which is used by the
  // platform implementation to choose the layout to map |dom_code| to.
  virtual ui::DomKey GetDomKeyFromDomCodeForLayout(
      ui::DomCode dom_code,
      uint32_t keyboard_layout_index) = 0;

 private:
  // Retrieves each writing system key from the layout associated with
  // |keyboard_layout_index| and populates |keyboard_layout| with the
  // corresponding dom key.
  void PopulateLayout(uint32_t keyboard_layout_index,
                      ui::DomKeyboardLayout* keyboard_layout);
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MAP_BASE_H_
