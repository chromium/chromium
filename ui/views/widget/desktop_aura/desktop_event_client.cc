// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_event_client.h"

#include "ui/aura/env.h"

namespace views {

DesktopEventClient::DesktopEventClient() = default;

DesktopEventClient::~DesktopEventClient() = default;

bool DesktopEventClient::CanProcessEventsWithinSubtree(
    const aura::Window* window) const {
  return true;
}

ui::EventTarget* DesktopEventClient::GetToplevelEventTarget() {
  return aura::Env::GetInstance();
}

}  // namespace views
