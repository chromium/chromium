// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LINUX_LINUX_DESKTOP_H_
#define UI_BASE_LINUX_LINUX_DESKTOP_H_

#include "base/component_export.h"

namespace base {
class Value;
}  // namespace base

namespace ui {

// Returns desktop environment info as list value.
COMPONENT_EXPORT(UI_BASE) base::Value GetDesktopEnvironmentInfoAsListValue();

}  // namespace ui

#endif  // UI_BASE_LINUX_LINUX_DESKTOP_H_
