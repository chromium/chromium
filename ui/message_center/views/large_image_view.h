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
 public:
  METADATA_HEADER(LargeImageView);

  explicit LargeImageView(const gfx::Size& max_size);
  LargeImageView(const LargeImageView&) = delete;
  LargeImageView& operator=(const LargeImageView&) = delete;
  ~LargeImageView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  void SetImage(const gfx::ImageSkia& image);

 private:
  // Calculates the resized image's size so that the resized image's width does
  // not exceed the threshold.
  gfx::Size CalculateResizedImageSizeForWidth();

  const gfx::Size max_size_;
  const gfx::Size min_size_;
  gfx::ImageSkia image_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_LARGE_IMAGE_VIEW_H_
