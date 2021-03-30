// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_THEME_TRACKING_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_THEME_TRACKING_IMAGE_VIEW_H_

#include "ui/views/controls/image_view.h"

namespace views {

// An ImageView that displays either |light_image| or |dark_image| based on the
// current background color returned by |get_background_color_callback|. Tracks
// theme changes so the image is always the correct version. |light_image| and
// |dark_image| must be of the same size. The |light_image| is set by default
// upon construction.
class VIEWS_EXPORT ThemeTrackingImageView : public ImageView {
 public:
  METADATA_HEADER(ThemeTrackingImageView);
  ThemeTrackingImageView(
      const gfx::ImageSkia& light_image,
      const gfx::ImageSkia& dark_image,
      const base::RepeatingCallback<SkColor()>& get_background_color_callback);
  ThemeTrackingImageView(const ThemeTrackingImageView&) = delete;
  ThemeTrackingImageView& operator=(const ThemeTrackingImageView&) = delete;
  ~ThemeTrackingImageView() override;

  // ImageView:
  void OnThemeChanged() override;

 private:
  // The underlying light and dark mode images.
  gfx::ImageSkia light_image_;
  gfx::ImageSkia dark_image_;

  base::RepeatingCallback<SkColor()> get_background_color_callback_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_THEME_TRACKING_IMAGE_VIEW_H_
