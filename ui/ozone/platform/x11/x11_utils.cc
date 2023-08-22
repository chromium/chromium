// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_utils.h"

#include "base/strings/string_util.h"
#include "ui/base/x/x11_util.h"

namespace ui {

X11Utils::X11Utils() = default;

X11Utils::~X11Utils() = default;

gfx::ImageSkia X11Utils::GetNativeWindowIcon(intptr_t target_window_id) {
  return ui::GetNativeWindowIcon(target_window_id);
}

std::string X11Utils::GetWmWindowClass(const std::string& desktop_base_name) {
  std::string window_class = desktop_base_name;
  if (!window_class.empty()) {
    // Capitalize the first character like gtk does.
    window_class[0] = base::ToUpperASCII(window_class[0]);
  }
  return window_class;
}

void X11Utils::OnUnhandledKeyEvent(const KeyEvent& key_event) {
  // Do nothing.
}

}  // namespace ui
