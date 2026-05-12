// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/begin_frame_source_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::BeginFrameSourceExtension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(BeginFrameSourceExtension*,
                             kBeginFrameSourceExtensionKey,
                             nullptr)

BeginFrameSourceExtension::~BeginFrameSourceExtension() = default;

void BeginFrameSourceExtension::SetBeginFrameSourceExtension(
    PlatformWindow* window,
    BeginFrameSourceExtension* source) {
  window->SetProperty(kBeginFrameSourceExtensionKey, source);
}

BeginFrameSourceExtension* GetBeginFrameSourceExtension(
    const PlatformWindow& window) {
  return window.GetProperty(kBeginFrameSourceExtensionKey);
}

}  // namespace ui
