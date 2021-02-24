// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/theme_tracking_image_view.h"

#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace views {

ThemeTrackingImageView::ThemeTrackingImageView(
    const gfx::ImageSkia& light_image,
    const gfx::ImageSkia& dark_image,
    const base::RepeatingCallback<SkColor()>& get_background_color_callback)
    : light_image_(light_image),
      dark_image_(dark_image),
      get_background_color_callback_(get_background_color_callback) {
  DCHECK_EQ(light_image.size(), dark_image.size());
  SetImage(light_image);
}

ThemeTrackingImageView::~ThemeTrackingImageView() = default;

void ThemeTrackingImageView::OnThemeChanged() {
  ImageView::OnThemeChanged();
  SetImage(color_utils::IsDark(get_background_color_callback_.Run())
               ? dark_image_
               : light_image_);
}

BEGIN_METADATA(ThemeTrackingImageView, views::ImageView)
END_METADATA

}  // namespace views
