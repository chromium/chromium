// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_THEME_MANAGER_H_
#define UI_BASE_CURSOR_CURSOR_THEME_MANAGER_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/base/cursor/cursor_theme_manager_observer.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CURSOR_THEME_MANAGER) CursorThemeManager {
 public:
  CursorThemeManager(const CursorThemeManager&) = delete;
  CursorThemeManager& operator=(const CursorThemeManager&) = delete;
  virtual ~CursorThemeManager();

  static CursorThemeManager* GetInstance();

  // Adds |observer| and makes initial OnCursorThemNameChanged() and/or
  // OnCursorThemeSizeChanged() calls if the respective settings were set.
  void AddObserver(CursorThemeManagerObserver* observer);

  void RemoveObserver(CursorThemeManagerObserver* observer);

  virtual std::string GetCursorThemeName() = 0;
  virtual int GetCursorThemeSize() = 0;

 protected:
  CursorThemeManager();

  const base::ObserverList<CursorThemeManagerObserver>&
  cursor_theme_observers() {
    return cursor_theme_observers_;
  }

 private:
  base::ObserverList<CursorThemeManagerObserver> cursor_theme_observers_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_THEME_MANAGER_H_
