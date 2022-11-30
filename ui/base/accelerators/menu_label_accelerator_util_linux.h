// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_LINUX_H_
#define UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_LINUX_H_

#include <string>

#include "base/component_export.h"

namespace ui {

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
COMPONENT_EXPORT(UI_BASE)
std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label);

// Removes the "&" accelerators from a Windows label.
COMPONENT_EXPORT(UI_BASE)
std::string RemoveWindowsStyleAccelerators(const std::string& label);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_LINUX_H_
