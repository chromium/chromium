// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_theme_manager_linux.h"

namespace ui {

// static
CursorThemeManagerLinux* CursorThemeManagerLinux::instance_ = nullptr;

// static
void CursorThemeManagerLinux::SetInstance(CursorThemeManagerLinux* instance) {
  instance_ = instance;
}

// static
CursorThemeManagerLinux* CursorThemeManagerLinux::GetInstance() {
  return instance_;
}

CursorThemeManagerLinux::CursorThemeManagerLinux() = default;

CursorThemeManagerLinux::~CursorThemeManagerLinux() = default;

void CursorThemeManagerLinux::AddObserver(
    CursorThemeManagerLinuxObserver* observer) {
  cursor_theme_observers_.AddObserver(observer);
}

void CursorThemeManagerLinux::RemoveObserver(
    CursorThemeManagerLinuxObserver* observer) {
  cursor_theme_observers_.RemoveObserver(observer);
}

}  // namespace ui
