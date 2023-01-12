// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/platform_window_cast.h"

#include "base/functional/bind.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

PlatformWindowCast::PlatformWindowCast(PlatformWindowDelegate* delegate,
                                       const gfx::Rect& bounds)
    : StubWindow(delegate, false, bounds) {
  gfx::AcceleratedWidget widget = (bounds.width() << 16) + bounds.height();
  delegate->OnAcceleratedWidgetAvailable(widget);

  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
}

PlatformWindowCast::~PlatformWindowCast() {
  if (PlatformEventSource::GetInstance())
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

bool PlatformWindowCast::CanDispatchEvent(const PlatformEvent& ne) {
  return true;
}

uint32_t PlatformWindowCast::DispatchEvent(const PlatformEvent& native_event) {
  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate())));

  return ui::POST_DISPATCH_STOP_PROPAGATION;
}

}  // namespace ui
