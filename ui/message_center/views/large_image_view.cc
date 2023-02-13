// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/large_image_view.h"

#include "ash/constants/ash_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/views/background.h"

namespace message_center {

LargeImageView::LargeImageView(const gfx::Size& max_size)
    : max_size_(max_size), min_size_(max_size_.width(), /*height=*/0) {
  SetID(NotificationViewBase::kLargeImageView);

  bool set_background = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  set_background = !ash::features::IsNotificationsRefreshEnabled();
#endif  // IS_CHROMEOS_ASH
  if (set_background) {
    SetBackground(views::CreateThemedSolidBackground(
        ui::kColorNotificationImageBackground));
  }
}

LargeImageView::~LargeImageView() = default;

void LargeImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size resized_size = CalculateResizedImageSizeForWidth();
  gfx::Size drawn_size = resized_size;
  drawn_size.SetToMin(max_size_);
  gfx::Rect drawn_bounds = GetContentsBounds();
  drawn_bounds.ClampToCenteredSize(drawn_size);

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image_, skia::ImageOperations::RESIZE_BEST, resized_size);

  // Cut off the overflown part.
  gfx::ImageSkia drawn_image = gfx::ImageSkiaOperations::ExtractSubset(
      resized_image, gfx::Rect(drawn_size));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::features::IsNotificationsRefreshEnabled()) {
    SkPath path;
    const SkScalar corner_radius = SkIntToScalar(kImageCornerRadius);
    const SkScalar kRadius[8] = {corner_radius, corner_radius, corner_radius,
                                 corner_radius, corner_radius, corner_radius,
                                 corner_radius, corner_radius};
    path.addRoundRect(gfx::RectToSkRect(drawn_bounds), kRadius);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    canvas->DrawImageInPath(drawn_image, drawn_bounds.x(), drawn_bounds.y(),
                            path, flags);
    return;
  }
#endif  // IS_CHROMEOS_ASH

  canvas->DrawImageInt(drawn_image, drawn_bounds.x(), drawn_bounds.y());
}

void LargeImageView::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
  gfx::Size preferred_size = CalculateResizedImageSizeForWidth();
  preferred_size.SetToMax(min_size_);
  preferred_size.SetToMin(max_size_);
  SetPreferredSize(preferred_size);
  SchedulePaint();
}

gfx::Size LargeImageView::CalculateResizedImageSizeForWidth() {
  gfx::Size original_size = image_.size();
  if (original_size.width() <= max_size_.width()) {
    return image_.size();
  }

  const double proportion =
      original_size.height() / static_cast<double>(original_size.width());
  gfx::Size resized_size;
  resized_size.SetSize(max_size_.width(), max_size_.width() * proportion);
  return resized_size;
}

BEGIN_METADATA(LargeImageView, views::View)
END_METADATA

}  // namespace message_center
