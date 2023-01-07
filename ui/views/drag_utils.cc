// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/drag_utils.h"

#include "ui/base/layout.h"
#include "ui/views/widget/widget.h"

namespace views {

float ScaleFactorForDragFromWidget(const Widget* widget) {
  float device_scale = 1.0f;
  if (widget && widget->GetNativeView()) {
    gfx::NativeView view = widget->GetNativeView();
    device_scale = ui::GetScaleFactorForNativeView(view);
  }
  return device_scale;
}

}  // namespace views
