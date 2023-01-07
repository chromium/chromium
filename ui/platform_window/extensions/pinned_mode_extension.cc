// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/pinned_mode_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::PinnedModeExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(PinnedModeExtension*,
                             kPinnedModeExtensionKey,
                             nullptr)

PinnedModeExtension::~PinnedModeExtension() = default;

void PinnedModeExtension::SetPinnedModeExtension(
    PlatformWindow* window,
    PinnedModeExtension* extension) {
  window->SetProperty(kPinnedModeExtensionKey, extension);
}

PinnedModeExtension* GetPinnedModeExtension(const PlatformWindow& window) {
  return window.GetProperty(kPinnedModeExtensionKey);
}

}  // namespace ui
