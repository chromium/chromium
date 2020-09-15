// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/keyboard_hook_x11.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/stl_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

static KeyboardHookX11* g_instance = nullptr;

// XGrabKey essentially requires the modifier mask to explicitly be specified.
// You can specify 'AnyModifier' however doing so means the call to XGrabKey
// will fail if that key has been grabbed with any combination of modifiers.
// A common practice is to call XGrabKey with each individual modifier mask to
// avoid that problem.
const uint32_t kModifierMasks[] = {0,         // No additional modifier.
                                   Mod2Mask,  // Num lock.
                                   LockMask,  // Caps lock.
                                   Mod5Mask,  // Scroll lock.
                                   Mod2Mask | LockMask,
                                   Mod2Mask | Mod5Mask,
                                   LockMask | Mod5Mask,
                                   Mod2Mask | LockMask | Mod5Mask};

// This is the set of keys to lock when the website requests that all keys be
// locked.
const DomCode kDomCodesForLockAllKeys[] = {
    DomCode::ESCAPE,        DomCode::CONTEXT_MENU, DomCode::CONTROL_LEFT,
    DomCode::SHIFT_LEFT,    DomCode::ALT_LEFT,     DomCode::META_LEFT,
    DomCode::CONTROL_RIGHT, DomCode::SHIFT_RIGHT,  DomCode::ALT_RIGHT,
    DomCode::META_RIGHT};

}  // namespace

KeyboardHookX11::KeyboardHookX11(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)),
      x_display_(gfx::GetXDisplay()),
      x_window_(accelerated_widget) {}

KeyboardHookX11::~KeyboardHookX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // Use XUngrabKeys for each key that has been grabbed.  XUngrabKeyboard
  // purportedly releases all keys when called and would not require the nested
  // loops, however in practice the keys are not actually released.
  for (int native_key_code : grabbed_keys_) {
    for (uint32_t modifier : kModifierMasks) {
      XUngrabKey(x_display_, native_key_code, modifier,
                 static_cast<uint32_t>(x_window_));
    }
  }
}

bool KeyboardHookX11::RegisterHook() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Only one instance of this class can be registered at a time.
  DCHECK(!g_instance);
  g_instance = this;

  if (dom_codes().has_value())
    CaptureSpecificKeys();
  else
    CaptureAllKeys();

  return true;
}

void KeyboardHookX11::CaptureAllKeys() {
  // We could have used the XGrabKeyboard API here instead of calling XGrabKeys
  // on a hard-coded set of shortcut keys.  Calling XGrabKeyboard would make
  // this work much simpler, however it has side-effects which prevents its use.
  // An example side-effect is that it prevents the lock screen from starting as
  // the screensaver process also calls XGrabKeyboard but will receive an error
  // since it was already grabbed by the window with KeyboardLock.
  for (size_t i = 0; i < base::size(kDomCodesForLockAllKeys); i++) {
    CaptureKeyForDomCode(kDomCodesForLockAllKeys[i]);
  }
}

void KeyboardHookX11::CaptureSpecificKeys() {
  for (DomCode dom_code : dom_codes().value()) {
    CaptureKeyForDomCode(dom_code);
  }
}

void KeyboardHookX11::CaptureKeyForDomCode(DomCode dom_code) {
  int native_key_code = KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  if (native_key_code == KeycodeConverter::InvalidNativeKeycode())
    return;

  for (uint32_t modifier : kModifierMasks) {
    // XGrabKey always returns 1 so we can't rely on the return value to
    // determine if the grab succeeded.  Errors are reported to the global
    // error handler for debugging purposes but are not used to judge success.
    XGrabKey(x_display_, native_key_code, modifier,
             static_cast<uint32_t>(x_window_),
             /*owner_events=*/false,
             /*pointer_mode=*/GrabModeAsync,
             /*keyboard_mode=*/GrabModeAsync);
  }

  grabbed_keys_.push_back(native_key_code);
}

}  // namespace ui
