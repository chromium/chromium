// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENTS_OZONE_H_
#define UI_EVENTS_OZONE_EVENTS_OZONE_H_

#include "base/callback.h"
#include "ui/events/events_export.h"
#include "ui/events/platform_event.h"

namespace ui {

class Event;

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
// Returns true iff the event was handled.
EVENTS_EXPORT bool DispatchEventFromNativeUiEvent(
    const PlatformEvent& native_event,
    base::OnceCallback<void(ui::Event*)> callback);

EVENTS_EXPORT void DisableNativeUiEventDispatchForTest();

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENTS_OZONE_H_
