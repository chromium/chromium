// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/check.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"

namespace ui {

DomKeyboardLayoutMapBase::DomKeyboardLayoutMapBase() = default;

DomKeyboardLayoutMapBase::~DomKeyboardLayoutMapBase() = default;

base::flat_map<std::string, std::string> DomKeyboardLayoutMapBase::Generate() {
  uint32_t keyboard_layout_count = GetKeyboardLayoutCount();
  if (!keyboard_layout_count)
    return {};

  std::unique_ptr<ui::DomKeyboardLayoutManager> keyboard_layout_manager =
      std::make_unique<ui::DomKeyboardLayoutManager>();

  for (size_t i = 0; i < keyboard_layout_count; i++) {
    DomKeyboardLayout* const dom_keyboard_layout =
        keyboard_layout_manager->GetLayout(i);
    PopulateLayout(i, dom_keyboard_layout);

    if (dom_keyboard_layout->IsAsciiCapable())
      return dom_keyboard_layout->GetMap();
  }

  return keyboard_layout_manager->GetFirstAsciiCapableLayout()->GetMap();
}

void DomKeyboardLayoutMapBase::PopulateLayout(uint32_t keyboard_layout_index,
                                              ui::DomKeyboardLayout* layout) {
  DCHECK(layout);

  for (size_t entry = 0; entry < ui::kWritingSystemKeyDomCodeEntries; entry++) {
    ui::DomCode dom_code = ui::writing_system_key_domcodes[entry];

    ui::DomKey dom_key =
        GetDomKeyFromDomCodeForLayout(dom_code, keyboard_layout_index);
    if (dom_key == ui::DomKey::NONE)
      continue;

    uint32_t unicode_value = 0;
    if (dom_key.IsCharacter())
      unicode_value = dom_key.ToCharacter();
    else if (dom_key.IsDeadKey())
      unicode_value = dom_key.ToDeadKeyCombiningCharacter();
    else if (dom_key == ui::DomKey::ZENKAKU_HANKAKU)
      // Placeholder for hankaku/zenkaku string.
      unicode_value = kHankakuZenkakuPlaceholder;

    if (unicode_value != 0)
      layout->AddKeyMapping(dom_code, unicode_value);
  }
}

}  // namespace ui
