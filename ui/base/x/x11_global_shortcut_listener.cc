// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/x/x11_global_shortcut_listener.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/connection.h"

namespace {

// The modifiers masks used for grabbing keys.  Due to XGrabKey only working on
// exact modifiers, we need to grab all key combinations including zero or more
// of the following: Num lock, Caps lock and Scroll lock.  So that we can make
// sure the behavior of global shortcuts is consistent on all platforms.
const x11::ModMask kModifiersMasks[] = {
    {},                  // No additional modifier.
    x11::ModMask::c_2,   // Num lock
    x11::ModMask::Lock,  // Caps lock
    x11::ModMask::c_5,   // Scroll lock
    x11::ModMask::c_2 | x11::ModMask::Lock,
    x11::ModMask::c_2 | x11::ModMask::c_5,
    x11::ModMask::Lock | x11::ModMask::c_5,
    x11::ModMask::c_2 | x11::ModMask::Lock | x11::ModMask::c_5};

x11::ModMask GetNativeModifiers(bool is_alt_down,
                                bool is_ctrl_down,
                                bool is_shift_down) {
  constexpr auto kNoMods = x11::ModMask{};
  return (is_shift_down ? x11::ModMask::Shift : kNoMods) |
         (is_ctrl_down ? x11::ModMask::Control : kNoMods) |
         (is_alt_down ? x11::ModMask::c_1 : kNoMods);
}

}  // namespace

namespace ui {

XGlobalShortcutListener::XGlobalShortcutListener()
    : connection_(x11::Connection::Get()), x_root_window_(GetX11RootWindow()) {}

XGlobalShortcutListener::~XGlobalShortcutListener() {
  if (is_listening_)
    StopListening();
}

void XGlobalShortcutListener::StartListening() {
  DCHECK(!is_listening_);  // Don't start twice.
  DCHECK(!registered_combinations_.empty());

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

  is_listening_ = true;
}

void XGlobalShortcutListener::StopListening() {
  DCHECK(is_listening_);  // No point if we are not already listening.
  DCHECK(registered_combinations_.empty());

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  is_listening_ = false;
}

bool XGlobalShortcutListener::CanDispatchEvent(const PlatformEvent& event) {
  return event->type() == EventType::kKeyPressed;
}

uint32_t XGlobalShortcutListener::DispatchEvent(const PlatformEvent& event) {
  CHECK_EQ(event->type(), EventType::kKeyPressed);
  OnKeyPressEvent(*event->AsKeyEvent());
  return POST_DISPATCH_NONE;
}

bool XGlobalShortcutListener::RegisterAccelerator(KeyboardCode key_code,
                                                  bool is_alt_down,
                                                  bool is_ctrl_down,
                                                  bool is_shift_down) {
  auto modifiers = GetNativeModifiers(is_alt_down, is_ctrl_down, is_shift_down);
  auto keysym = XKeysymForWindowsKeyCode(key_code, false);
  auto keycode = connection_->KeysymToKeycode(keysym);

  // Because XGrabKey only works on the exact modifiers mask, we should register
  // our hot keys with modifiers that we want to ignore, including Num lock,
  // Caps lock, Scroll lock. See comment about |kModifiersMasks|.
  x11::Future<void> grab_requests[std::size(kModifiersMasks)];
  for (size_t i = 0; i < std::size(kModifiersMasks); i++) {
    grab_requests[i] = connection_->GrabKey(
        {false, x_root_window_, modifiers | kModifiersMasks[i], keycode,
         x11::GrabMode::Async, x11::GrabMode::Async});
  }
  connection_->Flush();
  for (auto& grab_request : grab_requests) {
    if (grab_request.Sync().error) {
      // We may have part of the hotkeys registered, clean up.
      for (auto mask : kModifiersMasks)
        connection_->UngrabKey({keycode, x_root_window_, modifiers | mask});

      return false;
    }
  }

  registered_combinations_.insert(
      Accelerator(key_code, is_alt_down, is_ctrl_down, is_shift_down));

  return true;
}

void XGlobalShortcutListener::UnregisterAccelerator(KeyboardCode key_code,
                                                    bool is_alt_down,
                                                    bool is_ctrl_down,
                                                    bool is_shift_down) {
  auto modifiers = GetNativeModifiers(is_alt_down, is_ctrl_down, is_shift_down);
  auto keysym = XKeysymForWindowsKeyCode(key_code, false);
  auto keycode = connection_->KeysymToKeycode(keysym);

  for (auto mask : kModifiersMasks)
    connection_->UngrabKey({keycode, x_root_window_, modifiers | mask});

  registered_combinations_.erase(
      Accelerator(key_code, is_alt_down, is_ctrl_down, is_shift_down));
}

void XGlobalShortcutListener::OnKeyPressEvent(const KeyEvent& event) {
  DCHECK_EQ(event.type(), EventType::kKeyPressed);

  const KeyboardCode key_code = event.key_code();
  const bool is_alt_down = event.flags() & EF_ALT_DOWN;
  const bool is_ctrl_down = event.flags() & EF_CONTROL_DOWN;
  const bool is_shift_down = event.flags() & EF_SHIFT_DOWN;

  if (!base::Contains(
          registered_combinations_,
          Accelerator(key_code, is_alt_down, is_ctrl_down, is_shift_down))) {
    return;
  }

  OnKeyPressed(key_code, is_alt_down, is_ctrl_down, is_shift_down);
}

}  // namespace ui
