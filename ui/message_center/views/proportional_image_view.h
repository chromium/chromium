// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_PROPORTIONAL_IMAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_PROPORTIONAL_IMAGE_VIEW_H_

#include "ui/base/models/image_model.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/view.h"

namespace message_center {

// ProportionalImageViews scale and center their images while preserving their
// original proportions.
class MESSAGE_CENTER_EXPORT ProportionalImageView : public views::View {
  METADATA_HEADER(ProportionalImageView, views::View)

 public:
  explicit ProportionalImageView(const gfx::Size& view_size);
  ProportionalImageView(const ProportionalImageView&) = delete;
  ProportionalImageView& operator=(const ProportionalImageView&) = delete;
  ~ProportionalImageView() override;

  // |image| is scaled to fit within `view_size` and `max_image_size` while
  // maintaining its original aspect ratio. It is then centered within the view.
  // Applies rounded corners OnPaint if `apply_rounded_corners` is set.
  void SetImage(const ui::ImageModel& image,
                const gfx::Size& max_image_size,
                bool apply_rounded_corners = false);

  // Get the scaled size for the image that will be drawn inside
  // `ProportionalImageView`.
  gfx::Size GetImageDrawingSize();

  void set_apply_rounded_corners(bool apply_rounded_corners) {
    apply_rounded_corners_ = apply_rounded_corners;
  }

  const ui::ImageModel& image() const { return image_; }

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  ui::ImageModel image_;
  gfx::Size max_image_size_;
  // Whether to apply rounded corners OnPaint.
  bool apply_rounded_corners_ = false;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_PROPORTIONAL_IMAGE_VIEW_H_
