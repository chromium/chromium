// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_EVENT_CLIENT_H_
#define UI_AURA_CLIENT_EVENT_CLIENT_H_

#include "ui/aura/aura_export.h"

namespace ui {
class EventTarget;
}

namespace aura {
class Window;
namespace client {

// An interface implemented by an object that alters event processing.
class AURA_EXPORT EventClient {
 public:
  // Returns true if events can be processed by |window| or any of its children.
  virtual bool GetCanProcessEventsWithinSubtree(const Window* window) const = 0;

  // Returns the top level EventTarget for the current environment.
  virtual ui::EventTarget* GetToplevelEventTarget() = 0;

 protected:
  virtual ~EventClient() {}
};

// Sets/Gets the event client on the root Window.
AURA_EXPORT void SetEventClient(Window* root_window, EventClient* client);
AURA_EXPORT EventClient* GetEventClient(const Window* root_window);

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_CLIENT_EVENT_CLIENT_H_
