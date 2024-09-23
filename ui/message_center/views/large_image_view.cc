// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/large_image_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/message_center/views/notification_view_util.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/views/background.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace message_center {

LargeImageView::LargeImageView(const gfx::Size& max_size)
    : max_size_(max_size), min_size_(max_size_.width(), /*height=*/0) {
  SetID(NotificationViewBase::kLargeImageView);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  SetBackground(views::CreateThemedSolidBackground(
      ui::kColorNotificationImageBackground));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

LargeImageView::~LargeImageView() = default;

void LargeImageView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (!original_image_.isNull() && size() != previous_bounds.size()) {
    drawn_image_ = CalculateDrawnImage();
  }
}

void LargeImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (!drawn_image_.isNull()) {
    gfx::Rect drawn_bounds = GetContentsBounds();
    drawn_bounds.ClampToCenteredSize(drawn_image_.size());
    canvas->DrawImageInt(drawn_image_, drawn_bounds.x(), drawn_bounds.y());
  }
}

void LargeImageView::SetImage(const gfx::ImageSkia& image) {
  DCHECK(!image.isNull());
  original_image_ = image;
  drawn_image_ = CalculateDrawnImage();

  gfx::Size preferred_size = CalculateResizedImageSizeForWidth();
  preferred_size.SetToMax(min_size_);
  preferred_size.SetToMin(max_size_);
  SetPreferredSize(preferred_size);
  SchedulePaint();
}

gfx::Size LargeImageView::CalculateResizedImageSizeForWidth() const {
  DCHECK(!original_image_.isNull());

  gfx::Size original_size = original_image_.size();
  if (original_size.width() <= max_size_.width()) {
    return original_image_.size();
  }

  const double proportion =
      original_size.height() / static_cast<double>(original_size.width());
  gfx::Size resized_size;
  resized_size.SetSize(max_size_.width(), max_size_.width() * proportion);
  return resized_size;
}

gfx::ImageSkia LargeImageView::CalculateDrawnImage() const {
  DCHECK(!original_image_.isNull());

  // Resize `image` so that the image width does not exceed the restriction.
  const gfx::ImageSkia resized_image =
      gfx::ImageSkiaOperations::CreateResizedImage(
          original_image_, skia::ImageOperations::RESIZE_BEST,
          CalculateResizedImageSizeForWidth());

  // The height of the image could still be larger than that of `max_size`.
  // Therefore, cap the image size with `max_size`.
  gfx::Size clamed_size = resized_image.size();
  clamed_size.SetToMin(max_size_);

  // Crop out the image to draw.
  clamed_size.SetToMin(GetContentsBounds().size());
  const gfx::ImageSkia cropped_image = gfx::ImageSkiaOperations::ExtractSubset(
      resized_image, gfx::Rect(clamed_size));

  if (const std::optional<size_t> corner_radius =
          notification_view_util::GetLargeImageCornerRadius()) {
    // Return the cropped image decorated with rounded corners if necessary.
    return gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
        *corner_radius, cropped_image);
  }

  return cropped_image;
}

BEGIN_METADATA(LargeImageView)
END_METADATA

}  // namespace message_center
