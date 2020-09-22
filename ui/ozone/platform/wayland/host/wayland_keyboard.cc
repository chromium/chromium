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
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/keyboard_util_evdev.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
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

WaylandKeyboard::WaylandKeyboard(
    wl_keyboard* keyboard,
    zcr_keyboard_extension_v1* keyboard_extension_v1,
    WaylandConnection* connection,
    KeyboardLayoutEngine* layout_engine,
    Delegate* delegate)
    : obj_(keyboard),
      connection_(connection),
      delegate_(delegate),
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

  DCHECK(delegate_);
  delegate_->OnKeyboardCreated(this);

  wl_keyboard_add_listener(obj_.get(), &listener, this);
  // TODO(tonikitoo): Default auto-repeat to ON here?

  if (keyboard_extension_v1)
    extended_keyboard_v1_.reset(zcr_keyboard_extension_v1_get_extended_keyboard(
        keyboard_extension_v1, obj_.get()));
}

WaylandKeyboard::~WaylandKeyboard() {
  delegate_->OnKeyboardDestroyed(this);
}

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
  if (auto* window = wl::RootWindowFromWlSurface(surface)) {
    auto* self = static_cast<WaylandKeyboard*>(data);
    self->delegate_->OnKeyboardFocusChanged(window, /*focused=*/true);
  }
}

void WaylandKeyboard::Leave(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface) {
  // wl_surface might have been destroyed by this time.
  auto* self = static_cast<WaylandKeyboard*>(data);
  if (auto* window = wl::RootWindowFromWlSurface(surface))
    self->delegate_->OnKeyboardFocusChanged(window, /*focused=*/false);

  // Upon window focus lose, reset the key repeat timers.
  self->auto_repeat_handler_.StopKeyRepeat();
}

void WaylandKeyboard::Key(void* data,
                          wl_keyboard* obj,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  if (down)
    keyboard->connection_->set_serial(serial, ET_KEY_PRESSED);
  int device_id = keyboard->device_id();

  keyboard->auto_repeat_handler_.UpdateKeyRepeat(
      key, 0 /*scan_code*/, down, false /*suppress_auto_repeat*/, device_id);

  // TODO(tonikitoo,msisov): Handler 'repeat' parameter below.
  keyboard->DispatchKey(key, 0 /*scan_code*/, down, false /*repeat*/,
                        EventTimeForNow(), device_id, EF_NONE);
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

  int modifiers = keyboard->layout_engine_->UpdateModifiers(depressed, latched,
                                                            locked, group);
  keyboard->delegate_->OnKeyboardModifiersChanged(modifiers);
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
                                  uint32_t scan_code,
                                  bool down,
                                  bool repeat,
                                  base::TimeTicks timestamp,
                                  int device_id,
                                  int flags) {
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(EvdevCodeToNativeCode(key));
  if (dom_code == ui::DomCode::NONE)
    return;

  // Pass empty DomKey and KeyboardCode here so the delegate can pre-process
  // and decode it when needed.
  uint32_t result = delegate_->OnKeyboardKeyEvent(
      down ? ET_KEY_PRESSED : ET_KEY_RELEASED, dom_code, repeat, timestamp);

  if (extended_keyboard_v1_) {
    bool handled = result & POST_DISPATCH_STOP_PROPAGATION;
    zcr_extended_keyboard_v1_ack_key(extended_keyboard_v1_.get(),
                                     connection_->serial(), handled);
  }
}

bool WaylandKeyboard::Decode(DomCode dom_code,
                             int modifiers,
                             DomKey* out_dom_key,
                             KeyboardCode* out_key_code) {
  DCHECK(out_dom_key);
  DCHECK(out_key_code);
  return layout_engine_->Lookup(dom_code, modifiers, out_dom_key, out_key_code);
}

void WaylandKeyboard::SyncCallback(void* data,
                                   struct wl_callback* cb,
                                   uint32_t time) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);
  DCHECK(keyboard->auto_repeat_closure_);
  std::move(keyboard->auto_repeat_closure_).Run();
  keyboard->sync_callback_.reset();
}

}  // namespace ui
