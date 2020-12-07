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
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

static KeyboardHookX11* g_instance = nullptr;

// XGrabKey essentially requires the modifier mask to explicitly be specified.
// You can specify 'AnyModifier' however doing so means the call to XGrabKey
// will fail if that key has been grabbed with any combination of modifiers.
// A common practice is to call XGrabKey with each individual modifier mask to
// avoid that problem.
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

KeyboardHookX11::KeyboardHookX11(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    gfx::AcceleratedWidget accelerated_widget,
    KeyEventCallback callback)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)),
      connection_(x11::Connection::Get()),
      x_window_(static_cast<x11::Window>(accelerated_widget)) {}

KeyboardHookX11::~KeyboardHookX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // Use UngrabKey for each key that has been grabbed.  UngrabKeyboard
  // purportedly releases all keys when called and would not require the nested
  // loops, however in practice the keys are not actually released.
  for (int native_key_code : grabbed_keys_) {
    for (auto modifier : kModifierMasks) {
      connection_->UngrabKey(
          {static_cast<x11::KeyCode>(native_key_code), x_window_, modifier});
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
  for (auto kDomCodesForLockAllKey : kDomCodesForLockAllKeys)
    CaptureKeyForDomCode(kDomCodesForLockAllKey);
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

  for (auto modifier : kModifierMasks) {
    connection_->GrabKey({
        .owner_events = false,
        .grab_window = x_window_,
        .modifiers = modifier,
        .key = static_cast<x11::KeyCode>(native_key_code),
        .pointer_mode = x11::GrabMode::Async,
        .keyboard_mode = x11::GrabMode::Async,
    });
  }

  grabbed_keys_.push_back(native_key_code);
}

}  // namespace ui
