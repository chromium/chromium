// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_POSITION_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_POSITION_CLIENT_H_

#include "ui/views/views_export.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace views {

// Client that always offsets by the toplevel RootWindow of the passed
// in child NativeWidgetAura.
class VIEWS_EXPORT DesktopScreenPositionClient
    : public wm::DefaultScreenPositionClient {
 public:
  using DefaultScreenPositionClient::DefaultScreenPositionClient;

  DesktopScreenPositionClient(const DesktopScreenPositionClient&) = delete;
  DesktopScreenPositionClient& operator=(const DesktopScreenPositionClient&) =
      delete;

  ~DesktopScreenPositionClient() override;

  // aura::client::DefaultScreenPositionClient:
  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const display::Display& display) override;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_POSITION_CLIENT_H_
