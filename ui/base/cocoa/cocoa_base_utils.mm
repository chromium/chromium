// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/cocoa_base_utils.h"

#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/cocoa/cocoa_event_utils.h"

namespace ui {

ui::mojom::WindowOpenDisposition WindowOpenDispositionFromNSEvent(
    NSEvent* event) {
  return WindowOpenDispositionFromNSEventWithFlags(event, event.modifierFlags);
}

ui::mojom::WindowOpenDisposition WindowOpenDispositionFromNSEventWithFlags(
    NSEvent* event,
    NSUInteger modifiers) {
  int event_flags = EventFlagsFromNSEventWithModifiers(event, modifiers);
  return DispositionFromEventFlags(event_flags);
}

}  // namespace ui
