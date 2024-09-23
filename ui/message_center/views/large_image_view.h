// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_LARGE_IMAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_LARGE_IMAGE_VIEW_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace message_center {

// An image container view for notifications. Exported for tests.
class MESSAGE_CENTER_EXPORT LargeImageView : public views::View {
  METADATA_HEADER(LargeImageView, views::View)

 public:
  explicit LargeImageView(const gfx::Size& max_size);
  LargeImageView(const LargeImageView&) = delete;
  LargeImageView& operator=(const LargeImageView&) = delete;
  ~LargeImageView() override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnPaint(gfx::Canvas* canvas) override;

  void SetImage(const gfx::ImageSkia& image);

  const gfx::ImageSkia& original_image() const { return original_image_; }
  const gfx::ImageSkia& drawn_image() const { return drawn_image_; }

 private:
  // Calculates the resized image's size so that the resized image's width does
  // not exceed the threshold.
  gfx::Size CalculateResizedImageSizeForWidth() const;

  // Returns the image to draw as the notification large image.
  gfx::ImageSkia CalculateDrawnImage() const;

  const gfx::Size max_size_;
  const gfx::Size min_size_;

  // Caches the original image. Updates in `SetImage()`.
  gfx::ImageSkia original_image_;

  // Caches the image to draw in this view. Adapted from the original image.
  // Updates when either `original_image_` or the view size changes.
  gfx::ImageSkia drawn_image_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_LARGE_IMAGE_VIEW_H_
