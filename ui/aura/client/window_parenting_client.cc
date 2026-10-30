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
  CHECK(window, base::NotFatalUntil::M163);

  Window* root_window = window->GetRootWindow();
  CHECK(root_window, base::NotFatalUntil::M163);
  root_window->SetProperty(kRootWindowWindowParentingClientKey,
                           window_tree_client);
}

WindowParentingClient* GetWindowParentingClient(Window* window) {
  CHECK(window, base::NotFatalUntil::M163);
  Window* root_window = window->GetRootWindow();
  CHECK(root_window, base::NotFatalUntil::M163);
  WindowParentingClient* client =
      root_window->GetProperty(kRootWindowWindowParentingClientKey);
  CHECK(client, base::NotFatalUntil::M163);
  return client;
}

void ParentWindowWithContext(Window* window,
                             Window* context,
                             const gfx::Rect& screen_bounds,
                             const int64_t display_id) {
  CHECK(context, base::NotFatalUntil::M163);

  // |context| must be attached to a hierarchy with a WindowParentingClient.
  WindowParentingClient* client = GetWindowParentingClient(context);
  CHECK(client, base::NotFatalUntil::M163);
  Window* default_parent =
      client->GetDefaultParent(window, screen_bounds, display_id);
  default_parent->AddChild(window);
}

}  // namespace client
}  // namespace aura
