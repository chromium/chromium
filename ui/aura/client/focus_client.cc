// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/focus_client.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"

namespace aura {
namespace client {

void SetFocusClient(Window* root_window, FocusClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kFocusClientKey, client);
}

FocusClient* GetFocusClient(Window* window) {
  return GetFocusClient(static_cast<const Window*>(window));
}

FocusClient* GetFocusClient(const Window* window) {
  const Window* root_window = window->GetRootWindow();
  return root_window ? root_window->GetProperty(kFocusClientKey) : nullptr;
}

}  // namespace client
}  // namespace aura
