// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EVENT_INJECTOR_H_
#define UI_AURA_EVENT_INJECTOR_H_

#include "base/macros.h"
#include "ui/aura/aura_export.h"

namespace ui {
class Event;
struct EventDispatchDetails;
}

namespace aura {

class WindowTreeHost;

// Used to inject events as if they came from the OS.
class AURA_EXPORT EventInjector {
 public:
  EventInjector();
  ~EventInjector();

  // Inject |event| to |host|. If |event| is a LocatedEvent, then coordinates
  // are relative to host and in DIPs.
  ui::EventDispatchDetails Inject(WindowTreeHost* host, ui::Event* event);

 private:
  DISALLOW_COPY_AND_ASSIGN(EventInjector);
};

}  // namespace aura

#endif  // UI_AURA_EVENT_INJECTOR_H_
