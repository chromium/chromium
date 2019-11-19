// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_injector.h"

#include <utility>

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace aura {

EventInjector::EventInjector() = default;

EventInjector::~EventInjector() = default;

ui::EventDispatchDetails EventInjector::Inject(WindowTreeHost* host,
                                               ui::Event* event) {
  DCHECK(host);
  DCHECK(event);

  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    // Transforming the coordinate to the root will apply the screen scale
    // factor to the event's location and also the screen rotation degree.
    located_event->UpdateForRootTransform(
        host->GetRootTransform(),
        host->GetRootTransformForLocalEventCoordinates());
  }

  return host->event_sink()->OnEventFromSource(event);
}

}  // namespace aura
