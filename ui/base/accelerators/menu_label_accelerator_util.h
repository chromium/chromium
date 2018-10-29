// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_H_
#define UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"

namespace ui {

UI_BASE_EXPORT base::char16 GetMnemonic(const base::string16& label);

// This function escapes every '&' in label by replacing it with '&&', to avoid
// having single ampersands in user-provided strings treated as accelerators.
UI_BASE_EXPORT base::string16 EscapeMenuLabelAmpersands(
    const base::string16& label);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MENU_LABEL_ACCELERATOR_UTIL_H_
