// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/color_tracking_icon_view.h"

#include "ui/gfx/paint_vector_icon.h"

namespace views {

ColorTrackingIconView::ColorTrackingIconView(
    const gfx::VectorIcon& icon,
    int icon_size,
    ui::NativeTheme::ColorId icon_color_id)
    : icon_(icon), icon_size_(icon_size), icon_color_id_(icon_color_id) {
  // Set the image using the color generated from icon_color_id_. This will
  // allow the ImageView to report its preferred size before OnThemeChanged for
  // layout purposes.
  SetImage(gfx::CreateVectorIcon(
      icon_, icon_size_, GetNativeTheme()->GetSystemColor(icon_color_id_)));
}

void ColorTrackingIconView::OnThemeChanged() {
  ImageView::OnThemeChanged();
  SetImage(gfx::CreateVectorIcon(
      icon_, icon_size_, GetNativeTheme()->GetSystemColor(icon_color_id_)));
}

}  // namespace views
