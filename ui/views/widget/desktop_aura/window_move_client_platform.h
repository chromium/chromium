// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_MOVE_CLIENT_PLATFORM_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_MOVE_CLIENT_PLATFORM_H_

#include "ui/views/views_export.h"
#include "ui/wm/public/window_move_client.h"

namespace views {

class DesktopWindowTreeHostPlatform;

// Reroutes move loop requests to DesktopWindowTreeHostPlatform.
class VIEWS_EXPORT WindowMoveClientPlatform : public wm::WindowMoveClient {
 public:
  explicit WindowMoveClientPlatform(DesktopWindowTreeHostPlatform* host);
  WindowMoveClientPlatform(const WindowMoveClientPlatform& host) = delete;
  WindowMoveClientPlatform& operator=(const WindowMoveClientPlatform& host) =
      delete;
  ~WindowMoveClientPlatform() override;

  // Overridden from wm::WindowMoveClient:
  wm::WindowMoveResult RunMoveLoop(aura::Window* window,
                                   const gfx::Vector2d& drag_offset,
                                   wm::WindowMoveSource move_source) override;
  void EndMoveLoop() override;

 private:
  // The RunMoveLoop request is forwarded to this host.
  DesktopWindowTreeHostPlatform* host_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_WINDOW_MOVE_CLIENT_PLATFORM_H_
