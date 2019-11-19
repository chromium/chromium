// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_LINUX_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_LINUX_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_base.h"

namespace ui {

// Linux extensions to the PlatformWindowBase.
class COMPONENT_EXPORT(PLATFORM_WINDOW) PlatformWindowLinux
    : public PlatformWindowBase {
 public:
  PlatformWindowLinux();
  ~PlatformWindowLinux() override;

  // X11-specific.  Returns whether an XSync extension is available at the
  // current platform.
  virtual bool IsSyncExtensionAvailable() const;
  // X11-specific.  Handles CompleteSwapAfterResize event coming from the
  // compositor observer.
  virtual void OnCompleteSwapAfterResize();

  // X11-specific.  Returns the workspace the PlatformWindow is located in.
  virtual base::Optional<int> GetWorkspace() const;
  // X11-specific.  Sets the PlatformWindow to be visible on all workspaces.
  virtual void SetVisibleOnAllWorkspaces(bool always_visible);
  // X11-specific.  Returns true if the PlatformWindow is visible on all
  // workspaces.
  virtual bool IsVisibleOnAllWorkspaces() const;

  // X11-specific.  Returns the current bounds in terms of the X11 Root Window
  // including the borders provided by the window manager (if any).
  virtual gfx::Rect GetXRootWindowOuterBounds() const;

  // X11-specific.  Says if the X11 Root Window contains the point within its
  // set shape. If shape is not set, returns true.
  virtual bool ContainsPointInXRegion(const gfx::Point& point) const;

  // X11-specific.  Asks X11 to set transparency of the X11 Root Window. Not
  // used for Wayland as it uses alpha channel to blend a window instead.
  virtual void SetOpacityForXWindow(float opacity);

  // X11-specific.  Asks X11 to lower the Xwindow down the stack so that it does
  // not obscure any sibling windows.
  virtual void LowerXWindow();
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_LINUX_H_
