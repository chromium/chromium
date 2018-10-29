// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_IMAGE_VIEW_H_

#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view_base.h"

namespace gfx {
class Canvas;
}

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// ImageView class.
//
// An ImageView can display an image from an ImageSkia. If a size is provided,
// the ImageView will resize the provided image to fit if it is too big or will
// center the image if smaller. Otherwise, the preferred size matches the
// provided image size.
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT ImageView : public ImageViewBase {
 public:
  // Internal class name.
  static const char kViewClassName[];

  ImageView();
  ~ImageView() override;

  // Set the image that should be displayed.
  void SetImage(const gfx::ImageSkia& img);

  // Set the image that should be displayed from a pointer. Reset the image
  // if the pointer is NULL. The pointer contents is copied in the receiver's
  // image.
  void SetImage(const gfx::ImageSkia* image_skia);

  // Returns the image currently displayed, which can be empty if not set.
  // The returned image is still owned by the ImageView.
  const gfx::ImageSkia& GetImage() const;

  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 protected:
  // Overridden from ImageViewBase:
  gfx::Size GetImageSize() const override;

 private:
  friend class ImageViewTest;

  void OnPaintImage(gfx::Canvas* canvas);

  // Gets an ImageSkia to paint that has proper rep for |scale|.
  gfx::ImageSkia GetPaintImage(float scale);

  // Returns true if |img| is the same as the last image we painted. This is
  // intended to be a quick check, not exhaustive. In other words it's possible
  // for this to return false even though the images are in fact equal.
  bool IsImageEqual(const gfx::ImageSkia& img) const;

  // The underlying image.
  gfx::ImageSkia image_;

  // Caches the scaled image reps.
  gfx::ImageSkia scaled_image_;

  // Scale last painted at.
  float last_paint_scale_ = 0.f;

  // Address of bytes we last painted. This is used only for comparison, so its
  // safe to cache.
  void* last_painted_bitmap_pixels_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ImageView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
