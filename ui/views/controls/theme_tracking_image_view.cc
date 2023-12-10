// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/theme_tracking_image_view.h"

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"

namespace views {

ThemeTrackingImageView::ThemeTrackingImageView(
    const ui::ImageModel& light_image_model,
    const ui::ImageModel& dark_image_model,
    const base::RepeatingCallback<SkColor()>& get_background_color_callback)
    : light_image_model_(light_image_model),
      dark_image_model_(dark_image_model),
      get_background_color_callback_(get_background_color_callback) {
  DCHECK_EQ(light_image_model_.Size(), dark_image_model_.Size());
  SetImage(light_image_model_);
}

ThemeTrackingImageView::ThemeTrackingImageView(
    const gfx::ImageSkia& light_image,
    const gfx::ImageSkia& dark_image,
    const base::RepeatingCallback<SkColor()>& get_background_color_callback)
    : light_image_model_(ui::ImageModel::FromImageSkia(light_image)),
      dark_image_model_(ui::ImageModel::FromImageSkia(dark_image)),
      get_background_color_callback_(get_background_color_callback) {
  DCHECK_EQ(light_image_model_.Size(), dark_image_model_.Size());
  SetImage(light_image_model_);
}

ThemeTrackingImageView::~ThemeTrackingImageView() = default;

void ThemeTrackingImageView::OnThemeChanged() {
  ImageView::OnThemeChanged();
  SetImage(color_utils::IsDark(get_background_color_callback_.Run())
               ? dark_image_model_
               : light_image_model_);
}

void ThemeTrackingImageView::SetLightImage(
    const ui::ImageModel& light_image_model) {
  light_image_model_ = light_image_model;
  if (!color_utils::IsDark(get_background_color_callback_.Run()))
    SetImage(light_image_model_);
}

void ThemeTrackingImageView::SetDarkImage(
    const ui::ImageModel& dark_image_model) {
  dark_image_model_ = dark_image_model;
  if (color_utils::IsDark(get_background_color_callback_.Run()))
    SetImage(dark_image_model_);
}

BEGIN_METADATA(ThemeTrackingImageView)
END_METADATA

}  // namespace views
