// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_injector.h"

#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace aura {

EventInjector::EventInjector() {}

EventInjector::~EventInjector() {}

ui::EventDispatchDetails EventInjector::Inject(WindowTreeHost* host,
                                               ui::Event* event) {
  DCHECK(host);
  Env* env = host->window()->env();
  DCHECK(env);
  DCHECK(event);

  if (env->mode() == Env::Mode::LOCAL)
    return host->event_sink()->OnEventFromSource(event);

  if (event->IsLocatedEvent()) {
    // The ui-service expects events coming in to have a location matching the
    // root location. The non-ui-service code does this by way of
    // OnEventFromSource() ending up in LocatedEvent::UpdateForRootTransform(),
    // which reset the root_location to match the location.
    event->AsLocatedEvent()->set_root_location_f(
        event->AsLocatedEvent()->location_f());
  }

  if (!event_injector_) {
    env->window_tree_client_->connector()->BindInterface(
        ws::mojom::kServiceName, &event_injector_);
  }
  event_injector_->InjectEventNoAck(host->GetDisplayId(),
                                    ui::Event::Clone(*event));
  return ui::EventDispatchDetails();
}

}  // namespace aura
