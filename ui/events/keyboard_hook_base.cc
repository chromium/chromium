// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook_base.h"

#include <utility>

#include "base/containers/contains.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

KeyboardHookBase::KeyboardHookBase(
    std::optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback)
    : key_event_callback_(std::move(callback)),
      dom_codes_(std::move(dom_codes)) {
  DCHECK(key_event_callback_);
}

KeyboardHookBase::~KeyboardHookBase() = default;

bool KeyboardHookBase::IsKeyLocked(DomCode dom_code) const {
  return ShouldCaptureKeyEvent(dom_code);
}

bool KeyboardHookBase::RegisterHook() {
  NOTIMPLEMENTED();
  return false;
}

bool KeyboardHookBase::ShouldCaptureKeyEvent(DomCode dom_code) const {
  if (dom_code == DomCode::NONE)
    return false;

  return !dom_codes_ || base::Contains(dom_codes_.value(), dom_code);
}

void KeyboardHookBase::ForwardCapturedKeyEvent(KeyEvent* event) {
  key_event_callback_.Run(event);
}

}  // namespace ui
