// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_EVENT_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_EVENT_CLIENT_H_

#include "ui/aura/client/event_client.h"
#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT DesktopEventClient : public aura::client::EventClient {
 public:
  DesktopEventClient();

  DesktopEventClient(const DesktopEventClient&) = delete;
  DesktopEventClient& operator=(const DesktopEventClient&) = delete;

  ~DesktopEventClient() override;

  // Overridden from aura::client::EventClient:
  bool GetCanProcessEventsWithinSubtree(
      const aura::Window* window) const override;
  ui::EventTarget* GetToplevelEventTarget() override;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_EVENT_CLIENT_H_
