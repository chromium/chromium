// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_keyboard.h"

#include <sys/mman.h>

#include "base/files/scoped_file.h"
#include "ui/base/ui_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/layout_util.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

const int kXkbKeycodeOffset = 8;

}  // namespace

// static
const wl_callback_listener WaylandKeyboard::callback_listener_ = {
    WaylandKeyboard::SyncCallback,
};

WaylandKeyboard::WaylandKeyboard(wl_keyboard* keyboard,
                                 const EventDispatchCallback& callback)
    : obj_(keyboard), callback_(callback), auto_repeat_handler_(this) {
  static const wl_keyboard_listener listener = {
      &WaylandKeyboard::Keymap,    &WaylandKeyboard::Enter,
      &WaylandKeyboard::Leave,     &WaylandKeyboard::Key,
      &WaylandKeyboard::Modifiers, &WaylandKeyboard::RepeatInfo,
  };

#if BUILDFLAG(USE_XKBCOMMON)
  auto* engine = static_cast<WaylandXkbKeyboardLayoutEngine*>(
      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine());
  engine->SetEventModifiers(&event_modifiers_);
#endif

  wl_keyboard_add_listener(obj_.get(), &listener, this);

  // TODO(tonikitoo): Default auto-repeat to ON here?
}

WaylandKeyboard::~WaylandKeyboard() {}

void WaylandKeyboard::Keymap(void* data,
                             wl_keyboard* obj,
                             uint32_t format,
                             int32_t raw_fd,
                             uint32_t size) {
  base::ScopedFD fd(raw_fd);
  if (!data || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    return;

  char* keymap_str = reinterpret_cast<char*>(
      mmap(nullptr, size, PROT_READ, MAP_SHARED, fd.get(), 0));
  if (keymap_str == MAP_FAILED)
    return;

  bool success =
      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
          ->SetCurrentLayoutFromBuffer(keymap_str, strnlen(keymap_str, size));
  DCHECK(success) << "Failed to set the XKB keyboard mapping.";
  munmap(keymap_str, size);
}

void WaylandKeyboard::Enter(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface,
                            wl_array* keys) {
  // wl_surface might have been destroyed by this time.
  if (surface)
    WaylandWindow::FromSurface(surface)->set_keyboard_focus(true);
}

void WaylandKeyboard::Leave(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface) {
  // wl_surface might have been destroyed by this time.
  if (surface)
    WaylandWindow::FromSurface(surface)->set_keyboard_focus(false);

  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  // Upon window focus lose, reset the key repeat timers.
  keyboard->auto_repeat_handler_.StopKeyRepeat();
}

void WaylandKeyboard::Key(void* data,
                          wl_keyboard* obj,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  keyboard->connection_->set_serial(serial);

  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  int device_id = keyboard->obj_.id();

  keyboard->auto_repeat_handler_.UpdateKeyRepeat(
      key, down, false /*suppress_auto_repeat*/, device_id);

  // TODO(tonikitoo,msisov): Handler 'repeat' parameter below.
  keyboard->DispatchKey(key, down, false /*repeat*/, EventTimeForNow(),
                        device_id);
}

void WaylandKeyboard::Modifiers(void* data,
                                wl_keyboard* obj,
                                uint32_t serial,
                                uint32_t mods_depressed,
                                uint32_t mods_latched,
                                uint32_t mods_locked,
                                uint32_t group) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto* engine = static_cast<WaylandXkbKeyboardLayoutEngine*>(
      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine());
  engine->UpdateModifiers(mods_depressed, mods_latched, mods_locked, group);
#endif
}

void WaylandKeyboard::RepeatInfo(void* data,
                                 wl_keyboard* obj,
                                 int32_t rate,
                                 int32_t delay) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  keyboard->auto_repeat_handler_.SetAutoRepeatRate(
      base::TimeDelta::FromMilliseconds(delay),
      base::TimeDelta::FromMilliseconds(rate));
}

void WaylandKeyboard::FlushInput(base::OnceClosure closure) {
  if (sync_callback_)
    return;

  auto_repeat_closure_ = std::move(closure);

  // wl_display_sync gives a chance for any key "up" events to arrive.
  // With a well behaved wayland compositor this should ensure we never
  // get spurious repeats.
  sync_callback_.reset(wl_display_sync(connection_->display()));
  wl_callback_add_listener(sync_callback_.get(), &callback_listener_, this);
  wl_display_flush(connection_->display());
}

void WaylandKeyboard::DispatchKey(uint32_t key,
                                  bool down,
                                  bool repeat,
                                  base::TimeTicks timestamp,
                                  int device_id) {
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(key + kXkbKeycodeOffset);
  if (dom_code == ui::DomCode::NONE)
    return;

  uint8_t flags = event_modifiers_.GetModifierFlags();
  DomKey dom_key;
  KeyboardCode key_code;
  if (!KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
          dom_code, flags, &dom_key, &key_code))
    return;

  if (!repeat) {
    int flag = ModifierDomKeyToEventFlag(dom_key);
    UpdateModifier(flag, down);
  }

  ui::KeyEvent event(down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code,
                     dom_code, event_modifiers_.GetModifierFlags(), dom_key,
                     timestamp);
  event.set_source_device_id(device_id);
  callback_.Run(&event);
}

void WaylandKeyboard::SyncCallback(void* data,
                                   struct wl_callback* cb,
                                   uint32_t time) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  std::move(keyboard->auto_repeat_closure_).Run();
  DCHECK(keyboard->auto_repeat_closure_.is_null());
  keyboard->sync_callback_.reset();
}

void WaylandKeyboard::UpdateModifier(int modifier_flag, bool down) {
  if (modifier_flag == EF_NONE)
    return;

  int modifier = EventModifiers::GetModifierFromEventFlag(modifier_flag);
  if (modifier == MODIFIER_NONE)
    return;

  // This mimics KeyboardEvDev, which matches chrome/x11.
  // Currently EF_MOD3_DOWN means that the CapsLock key is currently down,
  // and EF_CAPS_LOCK_ON means the caps lock state is enabled (and the
  // key may or may not be down, but usually isn't). There does need to
  // to be two different flags, since the physical CapsLock key is subject
  // to remapping, but the caps lock state (which can be triggered in a
  // variety of ways) is not.
  if (modifier == MODIFIER_CAPS_LOCK)
    event_modifiers_.UpdateModifier(MODIFIER_MOD3, down);
  else
    event_modifiers_.UpdateModifier(modifier, down);
}

}  // namespace ui
