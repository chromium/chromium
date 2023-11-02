// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LINUX_LINUX_DESKTOP_H_
#define UI_BASE_LINUX_LINUX_DESKTOP_H_

#include "base/component_export.h"
#include "base/values.h"

namespace ui {

// Returns desktop environment info as list of values.
COMPONENT_EXPORT(UI_BASE) base::Value::List GetDesktopEnvironmentInfo();

}  // namespace ui

#endif  // UI_BASE_LINUX_LINUX_DESKTOP_H_
