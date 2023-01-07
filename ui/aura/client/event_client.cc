// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/event_client.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(aura::client::EventClient*)

namespace aura {
namespace client {

DEFINE_UI_CLASS_PROPERTY_KEY(EventClient*, kRootWindowEventClientKey, NULL)

void SetEventClient(Window* root_window, EventClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowEventClientKey, client);
}

EventClient* GetEventClient(const Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ?
      root_window->GetProperty(kRootWindowEventClientKey) : NULL;
}

}  // namespace client
}  // namespace aura
