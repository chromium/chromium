// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_WORKSPACE_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_WORKSPACE_EXTENSION_H_

#include <string>

#include "base/component_export.h"

namespace ui {

class PlatformWindow;
class WorkspaceExtensionDelegate;

// A workspace extension that platforms can use to add support for workspaces.
// This is intended to be used only in conjunction with a PlatformWindow and
// owned by a PlatformWindow owner. To avoid casts from the PlatformWindow to
// the WorkspaceExtension, a pointer to this interface must be set through
// "SetWorkspaceExtension".
class COMPONENT_EXPORT(PLATFORM_WINDOW) WorkspaceExtension {
 public:
  // Returns the workspace the PlatformWindow is located in.
  virtual std::string GetWorkspace() const = 0;

  // Sets the PlatformWindow to be visible on all workspaces.
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) = 0;

  // Returns true if the PlatformWindow is visible on all
  // workspaces.
  virtual bool IsVisibleOnAllWorkspaces() const = 0;

  // Sets the delegate that is notified if the window has changed the workspace
  // it's located in.
  virtual void SetWorkspaceExtensionDelegate(
      WorkspaceExtensionDelegate* delegate) = 0;

 protected:
  virtual ~WorkspaceExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetWorkspaceExtension(PlatformWindow* platform_window,
                             WorkspaceExtension* workspace_extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
WorkspaceExtension* GetWorkspaceExtension(
    const PlatformWindow& platform_window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_WORKSPACE_EXTENSION_H_
