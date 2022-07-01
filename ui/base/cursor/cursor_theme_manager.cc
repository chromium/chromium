// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_theme_manager.h"

#include "base/observer_list.h"

namespace ui {

namespace {

CursorThemeManager* g_instance = nullptr;

}

CursorThemeManager::~CursorThemeManager() = default;

// static
CursorThemeManager* CursorThemeManager::GetInstance() {
  return g_instance;
}

// static
void CursorThemeManager::SetInstance(CursorThemeManager* instance) {
  g_instance = instance;
}

void CursorThemeManager::AddObserver(CursorThemeManagerObserver* observer) {
  cursor_theme_observers_.AddObserver(observer);
  std::string name = GetCursorThemeName();
  if (!name.empty())
    observer->OnCursorThemeNameChanged(name);
  int size = GetCursorThemeSize();
  if (size)
    observer->OnCursorThemeSizeChanged(size);
}

void CursorThemeManager::RemoveObserver(CursorThemeManagerObserver* observer) {
  cursor_theme_observers_.RemoveObserver(observer);
}

CursorThemeManager::CursorThemeManager() = default;

}  // namespace ui
