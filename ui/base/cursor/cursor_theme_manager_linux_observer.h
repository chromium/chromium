// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_THEME_MANAGER_LINUX_OBSERVER_H_
#define UI_BASE_CURSOR_CURSOR_THEME_MANAGER_LINUX_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT CursorThemeManagerLinuxObserver
    : public base::CheckedObserver {
 public:
  virtual void OnCursorThemeNameChanged(
      const std::string& cursor_theme_name) = 0;
  virtual void OnCursorThemeSizeChanged(int cursor_theme_size) = 0;

 protected:
  ~CursorThemeManagerLinuxObserver() override = default;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_THEME_MANAGER_LINUX_OBSERVER_H_
