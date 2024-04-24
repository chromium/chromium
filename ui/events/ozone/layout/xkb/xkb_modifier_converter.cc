// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_modifier_converter.h"

#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/ranges/algorithm.h"
#include "ui/events/event_constants.h"

namespace ui {
namespace {

// Mapping from ui::EventFlags to XKB modifier names.
constexpr struct {
  int ui_flag;
  const char* xkb_name;
} kFlags[] = {
    {ui::EF_SHIFT_DOWN, XKB_MOD_NAME_SHIFT},
    {ui::EF_CONTROL_DOWN, XKB_MOD_NAME_CTRL},
    {ui::EF_ALT_DOWN, XKB_MOD_NAME_ALT},
    {ui::EF_COMMAND_DOWN, XKB_MOD_NAME_LOGO},
    {ui::EF_ALTGR_DOWN, "Mod5"},
    {ui::EF_MOD3_DOWN, "Mod3"},
    {ui::EF_CAPS_LOCK_ON, XKB_MOD_NAME_CAPS},
    {ui::EF_NUM_LOCK_ON, XKB_MOD_NAME_NUM},
};

}  // namespace

XkbModifierConverter::XkbModifierConverter(std::vector<std::string> names)
    : names_(std::move(names)) {}

XkbModifierConverter::XkbModifierConverter(XkbModifierConverter&& other) =
    default;

XkbModifierConverter& XkbModifierConverter::operator=(
    XkbModifierConverter&& other) = default;

XkbModifierConverter::~XkbModifierConverter() = default;

// static
XkbModifierConverter XkbModifierConverter::CreateFromKeymap(
    xkb_keymap* keymap) {
  std::vector<std::string> names;
  xkb_mod_index_t num_mods = xkb_keymap_num_mods(keymap);
  names.reserve(num_mods);
  for (xkb_mod_index_t i = 0; i < num_mods; ++i) {
    const char* name = xkb_keymap_mod_get_name(keymap, i);
    DCHECK(name);
    names.push_back(name);
  }
  return XkbModifierConverter(names);
}

xkb_mod_mask_t XkbModifierConverter::MaskFromNames(
    const std::vector<std::string_view>& names) const {
  xkb_mod_mask_t xkb_modifier_mask = 0;
  for (const auto& name : names)
    xkb_modifier_mask |= MaskFromName(name);
  return xkb_modifier_mask;
}

xkb_mod_mask_t XkbModifierConverter::MaskFromUiFlags(int flags) const {
  xkb_mod_mask_t xkb_modifier_mask = 0;
  for (const auto& mapping : kFlags) {
    if (!(flags & mapping.ui_flag))
      continue;
    xkb_modifier_mask |= MaskFromName(mapping.xkb_name);
  }
  return xkb_modifier_mask;
}

int XkbModifierConverter::UiFlagsFromMask(xkb_mod_mask_t mask) const {
  int ui_flags = 0;
  for (const auto& mapping : kFlags) {
    if (mask & MaskFromName(mapping.xkb_name))
      ui_flags |= mapping.ui_flag;
  }
  return ui_flags;
}

xkb_mod_mask_t XkbModifierConverter::MaskFromName(std::string_view name) const {
  auto it = base::ranges::find(names_, name);
  if (it == names_.end())
    return 0;
  return 1 << std::distance(names_.begin(), it);
}

}  // namespace ui
