// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/theme_tracking_animated_image_view.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"

namespace views {

ThemeTrackingAnimatedImageView::ThemeTrackingAnimatedImageView(
    int light_animation_lottie_id,
    int dark_animation_lottie_id,
    base::RepeatingCallback<ui::ColorVariant()> get_background_color_callback)
    : light_animation_lottie_id_(light_animation_lottie_id),
      dark_animation_lottie_id_(dark_animation_lottie_id),
      get_background_color_callback_(std::move(get_background_color_callback)) {
}

ThemeTrackingAnimatedImageView::~ThemeTrackingAnimatedImageView() = default;

void ThemeTrackingAnimatedImageView::OnThemeChanged() {
  AnimatedImageView::OnThemeChanged();
  const bool is_dark =
      color_utils::IsDark(get_background_color_callback_.Run().ResolveToSkColor(
          GetColorProvider()));
  UpdateAnimatedImage(is_dark ? dark_animation_lottie_id_
                              : light_animation_lottie_id_);
}

void ThemeTrackingAnimatedImageView::UpdateAnimatedImage(int lottie_id) {
  std::optional<std::vector<uint8_t>> lottie_bytes =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(lottie_id);
  scoped_refptr<cc::SkottieWrapper> skottie =
      cc::SkottieWrapper::UnsafeCreateSerializable(std::move(*lottie_bytes));
  SetAnimatedImage(std::make_unique<lottie::Animation>(skottie));
  SizeToPreferredSize();
  Play();
}

BEGIN_METADATA(ThemeTrackingAnimatedImageView)
END_METADATA

}  // namespace views
