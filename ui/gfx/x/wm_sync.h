// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_WM_SYNC_H_
#define UI_GFX_X_WM_SYNC_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;

// Synchronizes with the X11 window manager if a WM is running,
// otherwise synchronizes with the X11 server.
class COMPONENT_EXPORT(X11) WmSync final : public EventObserver {
 public:
  WmSync(Connection* connection, base::OnceClosure on_synced);

  WmSync(WmSync&&) = delete;
  WmSync& operator=(WmSync&&) = delete;

  ~WmSync() override;

 private:
  // EventObserver:
  void OnEvent(const Event& xevent) override;

  void Cleanup();

  const raw_ptr<Connection> connection_;
  base::OnceClosure on_synced_;

  Window window_ = Window::None;
  ScopedEventSelector window_events_;

  base::ScopedObservation<Connection, EventObserver> scoped_observation_{this};
};

}  // namespace x11

#endif  //  UI_GFX_X_WM_SYNC_H_
