// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/system_modal_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::SystemModalExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(SystemModalExtension*,
                             kSystemModalExtensionKey,
                             nullptr)

SystemModalExtension::~SystemModalExtension() = default;

void SystemModalExtension::SetSystemModalExtension(
    PlatformWindow* window,
    SystemModalExtension* extension) {
  window->SetProperty(kSystemModalExtensionKey, extension);
}

SystemModalExtension* GetSystemModalExtension(const PlatformWindow& window) {
  return window.GetProperty(kSystemModalExtensionKey);
}

}  // namespace ui
