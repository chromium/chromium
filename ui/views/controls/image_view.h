// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_IMAGE_VIEW_H_

#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

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
class VIEWS_EXPORT ImageView : public View {
 public:
  METADATA_HEADER(ImageView);

  enum class Alignment { kLeading, kCenter, kTrailing };

  ImageView();
  ~ImageView() override;

  // Set the image that should be displayed.
  void SetImage(const gfx::ImageSkia& img);

  // Set the image that should be displayed from a pointer. Reset the image
  // if the pointer is NULL. The pointer contents is copied in the receiver's
  // image.
  void SetImage(const gfx::ImageSkia* image_skia);

  // Sets the desired size of the image to be displayed.
  void SetImageSize(const gfx::Size& size);

  // Reset the image size to the current image dimensions.
  void ResetImageSize();

  // Returns the actual bounds of the visible image inside the view.
  gfx::Rect GetImageBounds() const;

  // Returns the image currently displayed, which can be empty if not set.
  // The returned image is still owned by the ImageView.
  const gfx::ImageSkia& GetImage() const;

  // Set / Get the horizontal alignment.
  void SetHorizontalAlignment(Alignment ha);
  Alignment GetHorizontalAlignment() const;

  // Set / Get the vertical alignment.
  void SetVerticalAlignment(Alignment va);
  Alignment GetVerticalAlignment() const;

  // Set / Get the accessible name text.
  void SetAccessibleName(const base::string16& name);
  const base::string16& GetAccessibleName() const;

  // Set the tooltip text.
  void set_tooltip_text(const base::string16& tooltip) {
    tooltip_text_ = tooltip;
  }

  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  gfx::Size CalculatePreferredSize() const override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void PreferredSizeChanged() override;

 protected:

 private:
  friend class ImageViewTest;

  void OnPaintImage(gfx::Canvas* canvas);

  // Gets an ImageSkia to paint that has proper rep for |scale|.
  gfx::ImageSkia GetPaintImage(float scale);

  // Returns true if |img| is the same as the last image we painted. This is
  // intended to be a quick check, not exhaustive. In other words it's possible
  // for this to return false even though the images are in fact equal.
  bool IsImageEqual(const gfx::ImageSkia& img) const;

  // Recomputes and updates the |image_origin_|.
  void UpdateImageOrigin();

  gfx::Size GetImageSize() const;

  // The origin of the image.
  gfx::Point image_origin_;

  // The current tooltip text.
  base::string16 tooltip_text_;

  // The current accessible name text.
  base::string16 accessible_name_;

  // Horizontal alignment.
  Alignment horizontal_alignment_ = Alignment::kCenter;

  // Vertical alignment.
  Alignment vertical_alignment_ = Alignment::kCenter;

  // The underlying image.
  gfx::ImageSkia image_;

  // Caches the scaled image reps.
  gfx::ImageSkia scaled_image_;

  // Scale last painted at.
  float last_paint_scale_ = 0.f;

  // Address of bytes we last painted. This is used only for comparison, so its
  // safe to cache.
  void* last_painted_bitmap_pixels_ = nullptr;

  // The requested image size.
  base::Optional<gfx::Size> image_size_;

  DISALLOW_COPY_AND_ASSIGN(ImageView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_IMAGE_VIEW_H_
