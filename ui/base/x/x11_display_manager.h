// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DISPLAY_MANAGER_H_
#define UI_BASE_X_X11_DISPLAY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/x/x11_workspace_handler.h"
#include "ui/display/display.h"
#include "ui/display/display_change_notifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/event.h"

namespace views {
class DesktopScreenX11Test;
}

namespace ui {
class X11ScreenOzoneTest;

////////////////////////////////////////////////////////////////////////////////
// XDisplayManager class
//
// Responsible for fetching and maintaining list of |display::Display|s
// representing X11 screens connected to the system. XRandR extension is used
// when version >= 1.3 is available, otherwise it falls back to
// |DefaultScreenOfDisplay| Xlib API.
//
// Scale Factor information and simple hooks are delegated to API clients
// through |XDisplayManager::Delegate| interface. To get notifications about
// dynamic display changes, clients must register |DisplayObserver| instances
// and feed |XDisplayManager| with |x11::Event|s.
//
// All bounds and size values are assumed to be expressed in pixels.
class COMPONENT_EXPORT(UI_BASE_X) XDisplayManager
    : public X11WorkspaceHandler::Delegate {
 public:
  class Delegate;

  explicit XDisplayManager(Delegate* delegate);

  XDisplayManager(const XDisplayManager&) = delete;
  XDisplayManager& operator=(const XDisplayManager&) = delete;

  ~XDisplayManager() override;

  void Init();
  bool IsXrandrAvailable() const;
  void OnEvent(const x11::Event& xev);
  void UpdateDisplayList();
  void DispatchDelayedDisplayListUpdate();
  const display::Display& GetPrimaryDisplay() const;

  void AddObserver(display::DisplayObserver* observer);
  void RemoveObserver(display::DisplayObserver* observer);

  const std::vector<display::Display>& displays() const { return displays_; }

  // Returns current workspace.
  std::string GetCurrentWorkspace();

 private:
  friend class ui::X11ScreenOzoneTest;
  friend class views::DesktopScreenX11Test;

  void SetDisplayList(std::vector<display::Display> displays,
                      size_t primary_display_index);
  void FetchDisplayList();

  // X11WorkspaceHandler override:
  void OnCurrentWorkspaceChanged(const std::string& new_workspace) override;

  const raw_ptr<Delegate> delegate_;
  std::vector<display::Display> displays_;
  display::DisplayChangeNotifier change_notifier_;

  const raw_ptr<x11::Connection> connection_;
  x11::Window x_root_window_;
  size_t primary_display_index_ = 0;

  // The task which fetches/updates display list info asynchronously.
  base::CancelableOnceClosure update_task_;

  X11WorkspaceHandler workspace_handler_;
};

class COMPONENT_EXPORT(UI_BASE_X) XDisplayManager::Delegate {
 public:
  virtual ~Delegate() = default;
  virtual void OnXDisplayListUpdated() = 0;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_DISPLAY_MANAGER_H_
