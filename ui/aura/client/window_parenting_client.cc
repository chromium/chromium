// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/window_parenting_client.h"

#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(aura::client::WindowParentingClient*)

namespace aura {
namespace client {

DEFINE_UI_CLASS_PROPERTY_KEY(WindowParentingClient*,
                             kRootWindowWindowParentingClientKey,
                             NULL)

void SetWindowParentingClient(Window* window,
                              WindowParentingClient* window_tree_client) {
  DCHECK(window);

  Window* root_window = window->GetRootWindow();
  DCHECK(root_window);
  root_window->SetProperty(kRootWindowWindowParentingClientKey,
                           window_tree_client);
}

WindowParentingClient* GetWindowParentingClient(Window* window) {
  DCHECK(window);
  Window* root_window = window->GetRootWindow();
  DCHECK(root_window);
  WindowParentingClient* client =
      root_window->GetProperty(kRootWindowWindowParentingClientKey);
  DCHECK(client);
  return client;
}

void ParentWindowWithContext(Window* window,
                             Window* context,
                             const gfx::Rect& screen_bounds,
                             const int64_t display_id) {
  DCHECK(context);

  // |context| must be attached to a hierarchy with a WindowParentingClient.
  WindowParentingClient* client = GetWindowParentingClient(context);
  DCHECK(client);
  Window* default_parent =
      client->GetDefaultParent(window, screen_bounds, display_id);
  default_parent->AddChild(window);
}

}  // namespace client
}  // namespace aura
