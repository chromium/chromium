// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_THEME_TRACKING_ANIMATED_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_THEME_TRACKING_ANIMATED_IMAGE_VIEW_H_

#include "base/functional/callback_forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_variant.h"
#include "ui/views/controls/animated_image_view.h"

namespace views {

// An AnimatedImageView that displays either `light_animation_lottie_id` or
// `dark_animation_lottie_id` based on the current background color returned by
// `get_background_color_callback`. Tracks theme changes so the image is always
// the correct version.
class VIEWS_EXPORT ThemeTrackingAnimatedImageView : public AnimatedImageView {
  METADATA_HEADER(ThemeTrackingAnimatedImageView, AnimatedImageView)

 public:
  ThemeTrackingAnimatedImageView(int light_animation_lottie_id,
                                 int dark_animation_lottie_id,
                                 base::RepeatingCallback<ui::ColorVariant()>
                                     get_background_color_callback);

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

  base::RepeatingCallback<ui::ColorVariant()> get_background_color_callback_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_THEME_TRACKING_ANIMATED_IMAGE_VIEW_H_
