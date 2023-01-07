// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/desk_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::DeskExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(DeskExtension*, kDeskExtensionKey, nullptr)

DeskExtension::~DeskExtension() = default;

void DeskExtension::SetDeskExtension(PlatformWindow* window,
                                     DeskExtension* extension) {
  window->SetProperty(kDeskExtensionKey, extension);
}

DeskExtension* GetDeskExtension(const PlatformWindow& window) {
  return window.GetProperty(kDeskExtensionKey);
}

}  // namespace ui
