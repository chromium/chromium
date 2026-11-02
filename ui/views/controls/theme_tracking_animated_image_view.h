// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_THEME_TRACKING_ANIMATED_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_THEME_TRACKING_ANIMATED_IMAGE_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_variant.h"
#include "ui/views/controls/animated_image_view.h"

namespace views {

// An AnimatedImageView that displays either `light_animation_lottie_id` or
// `dark_animation_lottie_id` based on `background_color`. Tracks theme changes
// so the image is always the correct version.
class VIEWS_EXPORT ThemeTrackingAnimatedImageView : public AnimatedImageView {
  METADATA_HEADER(ThemeTrackingAnimatedImageView, AnimatedImageView)

 public:
  ThemeTrackingAnimatedImageView(int light_animation_lottie_id,
                                 int dark_animation_lottie_id,
                                 ui::ColorVariant background_color);

  ThemeTrackingAnimatedImageView(const ThemeTrackingAnimatedImageView&) =
      delete;
  ThemeTrackingAnimatedImageView& operator=(
      const ThemeTrackingAnimatedImageView&) = delete;

  ~ThemeTrackingAnimatedImageView() override;

  // AnimatedImageView:
  void OnThemeChanged() override;

 private:
  void UpdateAnimatedImage(int lottie_id);

  // The underlying light and dark mode animated images' Lottie IDs.
  int light_animation_lottie_id_;
  int dark_animation_lottie_id_;

  ui::ColorVariant background_color_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_THEME_TRACKING_ANIMATED_IMAGE_VIEW_H_
