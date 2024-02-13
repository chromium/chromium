// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_keyboard_hook.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

static X11KeyboardHook* g_instance = nullptr;

// GrabKey essentially requires the modifier mask to explicitly be specified.
// One can specify 'x11::ModMask::Any' however doing so means the call to
// GrabKey will fail if that key has been grabbed with any combination of
// modifiers.  The common practice is to call GrabKey with each individual
// modifier mask to avoid that problem.
const x11::ModMask kModifierMasks[] = {
    {},                  // No additional modifier.
    x11::ModMask::c_2,   // Num lock
    x11::ModMask::Lock,  // Caps lock
    x11::ModMask::c_5,   // Scroll lock
    x11::ModMask::c_2 | x11::ModMask::Lock,
    x11::ModMask::c_2 | x11::ModMask::c_5,
    x11::ModMask::Lock | x11::ModMask::c_5,
    x11::ModMask::c_2 | x11::ModMask::Lock | x11::ModMask::c_5};

// This is the set of keys to lock when the website requests that all keys be
// locked.
const DomCode kDomCodesForLockAllKeys[] = {
    DomCode::ESCAPE,        DomCode::CONTEXT_MENU, DomCode::CONTROL_LEFT,
    DomCode::SHIFT_LEFT,    DomCode::ALT_LEFT,     DomCode::META_LEFT,
    DomCode::CONTROL_RIGHT, DomCode::SHIFT_RIGHT,  DomCode::ALT_RIGHT,
    DomCode::META_RIGHT};

}  // namespace

X11KeyboardHook::X11KeyboardHook(
    std::optional<base::flat_set<DomCode>> dom_codes,
    BaseKeyboardHook::KeyEventCallback callback,
    gfx::AcceleratedWidget accelerated_widget)
    : BaseKeyboardHook(std::move(dom_codes), std::move(callback)),
      connection_(x11::Connection::Get()),
      window_(static_cast<x11::Window>(accelerated_widget)) {
  RegisterHook(this->dom_codes());
}

X11KeyboardHook::~X11KeyboardHook() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // Use UngrabKey for each key that has been grabbed.  UngrabKeyboard
  // purportedly releases all keys when called and would not require the nested
  // loops, however in practice the keys are not actually released.
  for (int native_key_code : grabbed_keys_) {
    for (auto modifier : kModifierMasks) {
      connection_->UngrabKey(
          {static_cast<x11::KeyCode>(native_key_code), window_, modifier});
    }
  }
}

void X11KeyboardHook::RegisterHook(
    const std::optional<base::flat_set<DomCode>>& dom_codes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Only one instance of this class can be registered at a time.
  DCHECK(!g_instance);
  g_instance = this;

  if (dom_codes.has_value()) {
    CaptureSpecificKeys(dom_codes);
  } else {
    CaptureAllKeys();
  }
}

void X11KeyboardHook::CaptureAllKeys() {
  // We could have used the XGrabKeyboard API here instead of calling XGrabKeys
  // on a hard-coded set of shortcut keys.  Calling XGrabKeyboard would make
  // this work much simpler, however it has side-effects which prevents its use.
  // An example side-effect is that it prevents the lock screen from starting as
  // the screensaver process also calls XGrabKeyboard but will receive an error
  // since it was already grabbed by the window with KeyboardLock.
  for (auto kDomCodesForLockAllKey : kDomCodesForLockAllKeys) {
    CaptureKeyForDomCode(kDomCodesForLockAllKey);
  }
}

void X11KeyboardHook::CaptureSpecificKeys(
    const std::optional<base::flat_set<DomCode>>& dom_codes) {
  for (DomCode dom_code : dom_codes.value()) {
    CaptureKeyForDomCode(dom_code);
  }
}

void X11KeyboardHook::CaptureKeyForDomCode(DomCode dom_code) {
  int native_key_code = KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  if (native_key_code == KeycodeConverter::InvalidNativeKeycode()) {
    return;
  }

  for (auto modifier : kModifierMasks) {
    connection_->GrabKey({
        .owner_events = false,
        .grab_window = window_,
        .modifiers = modifier,
        .key = static_cast<x11::KeyCode>(native_key_code),
        .pointer_mode = x11::GrabMode::Async,
        .keyboard_mode = x11::GrabMode::Async,
    });
  }

  grabbed_keys_.push_back(native_key_code);
}

}  // namespace ui
