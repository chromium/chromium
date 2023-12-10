// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENTS_OZONE_H_
#define UI_EVENTS_OZONE_EVENTS_OZONE_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/platform_event.h"

namespace ui {

// Wrap a "native" ui::Event in another ui::Event & dispatch it.
//
// This is really unfortunate, but exists for two reasons:
//
//   1. Some of the ui::Event constructors depend on global state that
//   is only used when building from a "native" event. For example:
//   last_click_event_ is used when constructing MouseEvent from
//   NativeEvent to determine click count.
//
//   2. Events contain a reference to a "native event", which some code
//   depends on. The ui::Event might get mutated during dispatch, but
//   the native event won't. Some code depends on the fact that the
//   "native" version of the event is unmodified.
//
// We are trying to fix both of these issues, but in the meantime we
// define NativeEvent == ui::Event.
//
// Returns `ui::EventResult` for the `ui::Event`.
EVENTS_EXPORT EventResult
DispatchEventFromNativeUiEvent(const PlatformEvent& native_event,
                               base::OnceCallback<void(ui::Event*)> callback);

// Disable native level event handling including dispatch,
// capture or mouse movements for tests.
EVENTS_EXPORT void DisableNativeUiEventDispatchForTest();
EVENTS_EXPORT bool IsNativeUiEventDispatchDisabled();

// Event::Properties constants for IBus-GTK and fcitx-GTK.
// Both of them in async mode use gtk-specific XKeyEvent::state bits 24 and 25.
// 24 is handled and 25 is ignored.
// Note that they use more bits, but Chrome does not handle it now.
// cf)
// https://github.com/ibus/ibus/blob/dd4cc5b028c35f9bb8fa9d3bdc8f26bcdfc43d40/src/ibustypes.h#L88
// https://github.com/fcitx/fcitx/blob/289b2f674d95651d4e0d0c77a48e3a2f0da40efe/src/lib/fcitx-utils/keysym.h#L47
// https://mail.gnome.org/archives/gtk-devel-list/2013-June/msg00003.html
constexpr char kPropertyKeyboardImeFlag[] = "_keyevent_kbd_ime_flags_";
constexpr size_t kPropertyKeyboardImeFlagOffset = 24;
constexpr uint8_t kPropertyKeyboardImeFlagMask = 0x03;
// Handled is the 24-th bit.
constexpr uint8_t kPropertyKeyboardImeHandledFlag =
    1 << (24 - kPropertyKeyboardImeFlagOffset);
// Ignored is the 25-th bit.
constexpr uint8_t kPropertyKeyboardImeIgnoredFlag =
    1 << (25 - kPropertyKeyboardImeFlagOffset);

// Sets the `flags` as keyboard ime flag property to given `properties`.
EVENTS_EXPORT void SetKeyboardImeFlagProperty(KeyEvent::Properties* properties,
                                              uint8_t flags);

// Sets the keyboard ime flags to the given `event`'s properties.
// `flags` should be the bitwise-or of the flags defined just above.
EVENTS_EXPORT void SetKeyboardImeFlags(KeyEvent* event, uint8_t flags);

// Returns the keyboard ime flags for the given `event`. If it does not have,
// 0 will be returned.
EVENTS_EXPORT uint8_t GetKeyboardImeFlags(const KeyEvent& event);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENTS_OZONE_H_
