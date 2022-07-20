// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_BASE_H_
#define UI_LINUX_LINUX_UI_BASE_H_

#include "ui/linux/linux_ui.h"

namespace ui {

class LinuxUiBase : public LinuxUi {
 public:
  LinuxUiBase();
  ~LinuxUiBase() override;

  // LinuxUi:
  ui::NativeTheme* GetNativeTheme(bool use_system_theme) const override;
};

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_BASE_H_
