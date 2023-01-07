// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/workspace_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::WorkspaceExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(WorkspaceExtension*,
                             kWorkspaceExtensionKey,
                             nullptr)

WorkspaceExtension::~WorkspaceExtension() = default;

void WorkspaceExtension::SetWorkspaceExtension(
    PlatformWindow* platform_window,
    WorkspaceExtension* workspace_extension) {
  platform_window->SetProperty(kWorkspaceExtensionKey, workspace_extension);
}

WorkspaceExtension* GetWorkspaceExtension(
    const PlatformWindow& platform_window) {
  return platform_window.GetProperty(kWorkspaceExtensionKey);
}

}  // namespace ui
