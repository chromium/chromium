// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_THEME_MANAGER_LINUX_H_
#define UI_BASE_CURSOR_CURSOR_THEME_MANAGER_LINUX_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/cursor/cursor_theme_manager_linux_observer.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT CursorThemeManagerLinux {
 public:
  virtual ~CursorThemeManagerLinux();

  static void SetInstance(CursorThemeManagerLinux* instance);

  static CursorThemeManagerLinux* GetInstance();

  virtual std::string GetCursorThemeName() = 0;
  virtual int GetCursorThemeSize() = 0;
  void AddObserver(CursorThemeManagerLinuxObserver* observer);
  void RemoveObserver(CursorThemeManagerLinuxObserver* observer);

 protected:
  CursorThemeManagerLinux();

  const base::ObserverList<CursorThemeManagerLinuxObserver>&
  cursor_theme_observers() {
    return cursor_theme_observers_;
  }

 private:
  static CursorThemeManagerLinux* instance_;

  base::ObserverList<CursorThemeManagerLinuxObserver> cursor_theme_observers_;

  DISALLOW_COPY_AND_ASSIGN(CursorThemeManagerLinux);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_THEME_MANAGER_LINUX_H_
