// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_H_
#define UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_H_

#include <string>

#include "base/component_export.h"

namespace ui {

COMPONENT_EXPORT(UI_BASE) char16_t GetMnemonic(const std::u16string& label);

// This function escapes every '&' in label by replacing it with '&&', to avoid
// having single ampersands in user-provided strings treated as accelerators.
COMPONENT_EXPORT(UI_BASE)
std::u16string EscapeMenuLabelAmpersands(const std::u16string& label);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_H_
