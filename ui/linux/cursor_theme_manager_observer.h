// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_CURSOR_THEME_MANAGER_OBSERVER_H_
#define UI_LINUX_CURSOR_THEME_MANAGER_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"

namespace ui {

class CursorThemeManagerObserver : public base::CheckedObserver {
 public:
  // |cursor_theme_name| will be nonempty.
  virtual void OnCursorThemeNameChanged(
      const std::string& cursor_theme_name) = 0;

  // |cursor_theme_size| will be nonzero.
  virtual void OnCursorThemeSizeChanged(int cursor_theme_size) = 0;

 protected:
  ~CursorThemeManagerObserver() override = default;
};

}  // namespace ui

#endif  // UI_LINUX_CURSOR_THEME_MANAGER_OBSERVER_H_
