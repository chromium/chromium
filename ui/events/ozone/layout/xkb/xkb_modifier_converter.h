// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_MODIFIER_CONVERTER_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_MODIFIER_CONVERTER_H_

#include <xkbcommon/xkbcommon.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"

namespace ui {

// Handles the conversions about XKB modifiers, names and ui::EventFlags.
class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) XkbModifierConverter {
 public:
  // Instantiates the converter with the given names.
  // The names should be the XKB's modifier names in the order of bits.
  // E.g. If the passed names are {XKB_MOD_NAME_SHIFT, XKB_MOD_NAME_CAPS}
  // in this order, the LSB represents whether SHIFT is set, and the second
  // LSB (i.e. 0x2) represents whether CAPS is set.
  explicit XkbModifierConverter(std::vector<std::string> names);

  XkbModifierConverter(XkbModifierConverter&& other);
  XkbModifierConverter& operator=(XkbModifierConverter&& other);

  ~XkbModifierConverter();

  // Instantiates the converter from |xkb_keymap|.
  static XkbModifierConverter CreateFromKeymap(xkb_keymap* keymap);

  // Returns a bit-mask of the modifiers represented by the given |names|.
  xkb_mod_mask_t MaskFromNames(
      const std::vector<std::string_view>& names) const;

  // Returns the converted xkb_mod_mask_t corresponding to the given flags.
  // flags should be the bit-or of ui::EventFlags key modifiers.
  // All unrecognized bits set in flags will be ignored.
  xkb_mod_mask_t MaskFromUiFlags(int flags) const;

  // Returns ui::EventFlags converted from the mask.
  int UiFlagsFromMask(xkb_mod_mask_t mask) const;

 private:
  // Returns a bit mask of the xkb modifier of the given name.
  // If it is not known, returns 0.
  xkb_mod_index_t MaskFromName(std::string_view name) const;

  // Holds the list of modifier names. The position of the name corresponds
  // to the bit position. See constructor's example for details.
  std::vector<std::string> names_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_MODIFIER_CONVERTER_H_
