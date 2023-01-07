// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace ui {

namespace {

class DomKeyboardLayoutMapOzone : public DomKeyboardLayoutMapBase {
 public:
  DomKeyboardLayoutMapOzone();

  DomKeyboardLayoutMapOzone(const DomKeyboardLayoutMapOzone&) = delete;
  DomKeyboardLayoutMapOzone& operator=(const DomKeyboardLayoutMapOzone&) =
      delete;

  ~DomKeyboardLayoutMapOzone() override;

 private:
  // ui::DomKeyboardLayoutMapBase implementation.
  uint32_t GetKeyboardLayoutCount() override;
  ui::DomKey GetDomKeyFromDomCodeForLayout(
      ui::DomCode dom_code,
      uint32_t keyboard_layout_index) override;
};

DomKeyboardLayoutMapOzone::DomKeyboardLayoutMapOzone() = default;

DomKeyboardLayoutMapOzone::~DomKeyboardLayoutMapOzone() = default;

uint32_t DomKeyboardLayoutMapOzone::GetKeyboardLayoutCount() {
  // There is only one keyboard layout available on Ozone.
  return 1;
}

ui::DomKey DomKeyboardLayoutMapOzone::GetDomKeyFromDomCodeForLayout(
    ui::DomCode dom_code,
    uint32_t keyboard_layout_index) {
  DCHECK_NE(dom_code, ui::DomCode::NONE);
  DCHECK_EQ(keyboard_layout_index, 0U);

  ui::KeyboardLayoutEngine* const keyboard_layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();

  ui::DomKey dom_key;
  ui::KeyboardCode unused_code;
  if (keyboard_layout_engine->Lookup(dom_code, 0, &dom_key, &unused_code))
    return dom_key;

  return ui::DomKey::NONE;
}

}  // namespace

// static
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap() {
  return DomKeyboardLayoutMapOzone().Generate();
}

}  // namespace ui
