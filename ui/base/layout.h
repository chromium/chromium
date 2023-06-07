// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LAYOUT_H_
#define UI_BASE_LAYOUT_H_

#include "base/component_export.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// Returns the ResourceScaleFactor used by `view`.
COMPONENT_EXPORT(UI_BASE)
float GetScaleFactorForNativeView(gfx::NativeView view);

}  // namespace ui

#endif  // UI_BASE_LAYOUT_H_
