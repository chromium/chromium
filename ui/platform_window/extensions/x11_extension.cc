// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/extensions/x11_extension.h"

#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::X11Extension*)

namespace ui {

DEFINE_UI_CLASS_PROPERTY_KEY(X11Extension*, kX11ExtensionKey, nullptr)

X11Extension::~X11Extension() = default;

void X11Extension::SetX11Extension(PlatformWindow* platform_window,
                                   X11Extension* x11_extension) {
  platform_window->SetProperty(kX11ExtensionKey, x11_extension);
}

X11Extension* GetX11Extension(const PlatformWindow& platform_window) {
  return platform_window.GetProperty(kX11ExtensionKey);
}

}  // namespace ui
