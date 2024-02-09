// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/base_keyboard_hook.h"

#include <utility>

#include "base/containers/contains.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

BaseKeyboardHook::BaseKeyboardHook(
    std::optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback)
    : key_event_callback_(std::move(callback)),
      dom_codes_(std::move(dom_codes)) {
  DCHECK(key_event_callback_);
}

BaseKeyboardHook::~BaseKeyboardHook() = default;

bool BaseKeyboardHook::IsKeyLocked(DomCode dom_code) const {
  return ShouldCaptureKeyEvent(dom_code);
}

bool BaseKeyboardHook::ShouldCaptureKeyEvent(DomCode dom_code) const {
  if (dom_code == DomCode::NONE)
    return false;

  return !dom_codes_ || base::Contains(dom_codes_.value(), dom_code);
}

void BaseKeyboardHook::ForwardCapturedKeyEvent(KeyEvent* event) {
  key_event_callback_.Run(event);
}

}  // namespace ui