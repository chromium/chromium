// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"

#include <utility>

#include "base/files/scoped_file.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "ui/base/buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/evdev/keyboard_util_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

namespace ui {

// static
const wl_callback_listener WaylandKeyboard::callback_listener_ = {
    WaylandKeyboard::SyncCallback,
};

WaylandKeyboard::WaylandKeyboard(wl_keyboard* keyboard,
                                 KeyboardLayoutEngine* layout_engine,
                                 const EventDispatchCallback& callback)
    : obj_(keyboard),
      callback_(callback),
      auto_repeat_handler_(this),
#if BUILDFLAG(USE_XKBCOMMON)
      layout_engine_(static_cast<XkbKeyboardLayoutEngine*>(layout_engine)) {
#else
      layout_engine_(layout_engine) {
#endif
  static const wl_keyboard_listener listener = {
      &WaylandKeyboard::Keymap,    &WaylandKeyboard::Enter,
      &WaylandKeyboard::Leave,     &WaylandKeyboard::Key,
      &WaylandKeyboard::Modifiers, &WaylandKeyboard::RepeatInfo,
  };

  wl_keyboard_add_listener(obj_.get(), &listener, this);

  // TODO(tonikitoo): Default auto-repeat to ON here?
}

WaylandKeyboard::~WaylandKeyboard() {}

void WaylandKeyboard::Keymap(void* data,
                             wl_keyboard* obj,
                             uint32_t format,
                             int32_t keymap_fd,
                             uint32_t size) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  base::ScopedFD fd(keymap_fd);
  auto length = size - 1;
  auto shmen = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd), base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
      length, base::UnguessableToken::Create());
  auto mapped_memory =
      base::UnsafeSharedMemoryRegion::Deserialize(std::move(shmen)).Map();
  const char* keymap = mapped_memory.GetMemoryAs<char>();

  if (!keymap || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    return;

  bool success = keyboard->layout_engine_->SetCurrentLayoutFromBuffer(
      keymap, mapped_memory.size());
  DCHECK(success) << "Failed to set the XKB keyboard mapping.";
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
                                uint32_t depressed,
                                uint32_t latched,
                                uint32_t locked,
                                uint32_t group) {
#if BUILDFLAG(USE_XKBCOMMON)
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  keyboard->modifiers_ = keyboard->layout_engine_->UpdateModifiers(
      depressed, latched, locked, group);
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
  connection_->ScheduleFlush();
}

void WaylandKeyboard::DispatchKey(uint32_t key,
                                  bool down,
                                  bool repeat,
                                  base::TimeTicks timestamp,
                                  int device_id) {
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(EvdevCodeToNativeCode(key));
  if (dom_code == ui::DomCode::NONE)
    return;

  DomKey dom_key;
  KeyboardCode key_code;
  if (!layout_engine_->Lookup(dom_code, modifiers_, &dom_key, &key_code))
    return;

  if (!repeat) {
    int flag = ModifierDomKeyToEventFlag(dom_key);
    UpdateModifier(flag, down);
  }

  ui::KeyEvent event(down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code,
                     dom_code, modifiers_, dom_key, timestamp);
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

void WaylandKeyboard::UpdateModifier(int modifier, bool down) {
  if (modifier == EF_NONE)
    return;

  // TODO(nickdiego): ChromeOS-specific keyboard remapping logic.
  // Remove this once it is properly guarded under OS_CHROMEOS.
  //
  // Currently EF_MOD3_DOWN means that the CapsLock key is currently down,
  // and EF_CAPS_LOCK_ON means the caps lock state is enabled (and the
  // key may or may not be down, but usually isn't). There does need to
  // to be two different flags, since the physical CapsLock key is subject
  // to remapping, but the caps lock state (which can be triggered in a
  // variety of ways) is not.
  if (modifier == EF_CAPS_LOCK_ON)
    modifier = (modifier & ~EF_CAPS_LOCK_ON) | EF_MOD3_DOWN;
  modifiers_ = down ? (modifiers_ | modifier) : (modifiers_ & ~modifier);
}

}  // namespace ui
