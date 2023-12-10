// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_WORKSPACE_HANDLER_H_
#define UI_BASE_X_X11_WORKSPACE_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {
class ScopedEventSelector;
}

namespace ui {

// Listens for global workspace changes and notifies observers.
class COMPONENT_EXPORT(UI_BASE_X) X11WorkspaceHandler
    : public x11::EventObserver {
 public:
  class Delegate {
   public:
    // Called when the workspace ID changes to|new_workspace|.
    virtual void OnCurrentWorkspaceChanged(
        const std::string& new_workspace) = 0;

   protected:
    virtual ~Delegate() = default;
  };
  explicit X11WorkspaceHandler(Delegate* delegate);
  ~X11WorkspaceHandler() override;
  X11WorkspaceHandler(const X11WorkspaceHandler&) = delete;
  X11WorkspaceHandler& operator=(const X11WorkspaceHandler&) = delete;

  // Gets the current workspace ID.
  std::string GetCurrentWorkspace();

 private:
  // x11::EventObserver
  void OnEvent(const x11::Event& event) override;

  void OnWorkspaceResponse(x11::GetPropertyResponse response);

  // The native root window.
  x11::Window x_root_window_;

  // Events selected on x_root_window_.
  x11::ScopedEventSelector x_root_window_events_;

  std::string workspace_;

  const raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<X11WorkspaceHandler> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_X_X11_WORKSPACE_HANDLER_H_
