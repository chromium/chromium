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
  ui::KeyboardLayoutEngine* const keyboard_layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  return keyboard_layout_engine
             ? keyboard_layout_engine->GetKeyboardLayoutCount()
             : 0;
}

ui::DomKey DomKeyboardLayoutMapOzone::GetDomKeyFromDomCodeForLayout(
    ui::DomCode dom_code,
    uint32_t keyboard_layout_index) {
  CHECK_NE(dom_code, ui::DomCode::NONE, base::NotFatalUntil::M163);

  ui::KeyboardLayoutEngine* const keyboard_layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!keyboard_layout_engine) {
    return ui::DomKey::NONE;
  }
  return keyboard_layout_engine->LookupForLayout(dom_code,
                                                 keyboard_layout_index);
}

}  // namespace

// static
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap() {
  return DomKeyboardLayoutMapOzone().Generate();
}

}  // namespace ui
