// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/color_tracking_icon_view.h"

#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

namespace views {

ColorTrackingIconView::ColorTrackingIconView(const gfx::VectorIcon& icon,
                                             int icon_size)
    : icon_(icon), icon_size_(icon_size) {}

void ColorTrackingIconView::OnThemeChanged() {
  ImageView::OnThemeChanged();
  const SkColor color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  SetImage(gfx::CreateVectorIcon(icon_, icon_size_, color));
}

}  // namespace views
