// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui_base.h"

#include "ui/native_theme/native_theme.h"

namespace ui {

LinuxUiBase::LinuxUiBase() = default;

LinuxUiBase::~LinuxUiBase() = default;

ui::NativeTheme* LinuxUiBase::GetNativeTheme(bool use_system_theme) const {
  return use_system_theme ? GetNativeThemeImpl()
                          : ui::NativeTheme::GetInstanceForNativeUi();
}

}  // namespace ui
