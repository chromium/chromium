// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/scoped_simple_keyboard_hook.h"

#include <utility>

#include "base/stl_util.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace aura {

ScopedSimpleKeyboardHook::ScopedSimpleKeyboardHook(
    base::Optional<base::flat_set<ui::DomCode>> dom_codes)
    : dom_codes_(std::move(dom_codes)) {}

ScopedSimpleKeyboardHook::~ScopedSimpleKeyboardHook() = default;

bool ScopedSimpleKeyboardHook::IsKeyLocked(ui::DomCode dom_code) {
  if (dom_code == ui::DomCode::NONE)
    return false;

  return !dom_codes_ || base::Contains(dom_codes_.value(), dom_code);
}

}  // namespace aura
