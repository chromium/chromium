// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_theme_manager.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ui {

namespace {

CursorThemeManager* g_instance = nullptr;

}

CursorThemeManager::~CursorThemeManager() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CursorThemeManager* CursorThemeManager::GetInstance() {
  return g_instance;
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

CursorThemeManager::CursorThemeManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

}  // namespace ui
