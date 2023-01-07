// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_WORKSPACE_EXTENSION_DELEGATE_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_WORKSPACE_EXTENSION_DELEGATE_H_

#include "base/component_export.h"

namespace ui {

// Notifies the delegate about changed workspace. The delegate must be set in
// WorkspaceExtension to be able to receive these changes.
class COMPONENT_EXPORT(PLATFORM_WINDOW) WorkspaceExtensionDelegate {
 public:
  // Notifies the delegate if the window has changed the workspace it is
  // located in.
  virtual void OnWorkspaceChanged() = 0;

 protected:
  virtual ~WorkspaceExtensionDelegate() = default;
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_WORKSPACE_EXTENSION_DELEGATE_H_