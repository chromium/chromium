// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_IMAGE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/image_view_base.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/views_export.h"

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
  METADATA_HEADER(ImageView, ImageViewBase)

 public:
  ImageView();
  explicit ImageView(const ui::ImageModel& image_model);

  ImageView(const ImageView&) = delete;
  ImageView& operator=(const ImageView&) = delete;

  ~ImageView() override;

  // Sets the image that should be displayed.
  void SetImage(const ui::ImageModel& image_model);

  // Returns the image currently displayed, which can be empty if not set.
  // TODO(pkasting): Convert to an ImageModel getter.
  gfx::ImageSkia GetImage() const;

  ui::ImageModel GetImageModel() const;

  // Set or get the `corner_radius`.
  void SetCornerRadius(int corner_radius);
  int GetCornerRadius() const;

  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override;

 protected:
  // Overridden from ImageViewBase:
  gfx::Size GetImageSize() const override;

  void OnThemeChanged() override;

 private:
  friend class ImageViewTest;

  void OnPaintImage(gfx::Canvas* canvas);

  // Gets an ImageSkia to paint that has proper rep for |scale|. Note that if
  // there is no existing rep of `scale`, we will utilize the image resize
  // operation to create one. The resize may be time consuming for a big image.
  gfx::ImageSkia GetPaintImage(float scale);

  // Returns true if |image_model| is the same as the last image we painted.
  // This is intended to be a quick check, not exhaustive. In other words it's
  // possible for this to return false even though the images are in fact equal.
  bool IsImageEqual(const ui::ImageModel& image_model) const;

  // The underlying image.
  ui::ImageModel image_model_;

  // Caches the scaled image reps.
  gfx::ImageSkia scaled_image_;

  int corner_radius_ = 0;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ImageView, ImageViewBase)
VIEW_BUILDER_OVERLOAD_METHOD(SetImage, const ui::ImageModel&)
VIEW_BUILDER_PROPERTY(int, CornerRadius)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ImageView)

#endif  // UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
