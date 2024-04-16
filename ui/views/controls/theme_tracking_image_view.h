// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_THEME_TRACKING_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_THEME_TRACKING_IMAGE_VIEW_H_

#include "base/functional/callback.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view.h"

namespace views {

// An ImageView that displays either `light_image` or `dark_image` based on the
// current background color returned by `get_background_color_callback`. Tracks
// theme changes so the image is always the correct version. `light_image` and
// `dark_image` must be of the same size. The `light_image` is set by default
// upon construction.
class VIEWS_EXPORT ThemeTrackingImageView : public ImageView {
  METADATA_HEADER(ThemeTrackingImageView, ImageView)

 public:
  ThemeTrackingImageView(
      const ui::ImageModel& light_image_model,
      const ui::ImageModel& dark_image_model,
      const base::RepeatingCallback<SkColor()>& get_background_color_callback);

  // TODO(crbug.com/40239900): Remove this constructor and migrate existing
  // callers to `ImageModel`.
  ThemeTrackingImageView(
      const gfx::ImageSkia& light_image,
      const gfx::ImageSkia& dark_image,
      const base::RepeatingCallback<SkColor()>& get_background_color_callback);

  ThemeTrackingImageView(const ThemeTrackingImageView&) = delete;
  ThemeTrackingImageView& operator=(const ThemeTrackingImageView&) = delete;
  ~ThemeTrackingImageView() override;

  void SetLightImage(const ui::ImageModel& light_image_model);
  void SetDarkImage(const ui::ImageModel& dark_image_model);

  // ImageView:
  void OnThemeChanged() override;

 private:
  // The underlying light and dark mode image models.
  ui::ImageModel light_image_model_;
  ui::ImageModel dark_image_model_;

  base::RepeatingCallback<SkColor()> get_background_color_callback_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_THEME_TRACKING_IMAGE_VIEW_H_
