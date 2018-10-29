// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_IMAGE_VIEW_BASE_H_
#define UI_VIEWS_CONTROLS_IMAGE_VIEW_BASE_H_

#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {

class VIEWS_EXPORT ImageViewBase : public View {
 public:
  enum Alignment { LEADING, CENTER, TRAILING };

  ImageViewBase();
  ~ImageViewBase() override;

  // Set the desired image size for the receiving ImageView.
  void SetImageSize(const gfx::Size& image_size);

  // Returns the actual bounds of the visible image inside the view.
  gfx::Rect GetImageBounds() const;

  // Reset the image size to the current image dimensions.
  void ResetImageSize();

  // Set / Get the horizontal alignment.
  void SetHorizontalAlignment(Alignment ha);
  Alignment GetHorizontalAlignment() const;

  // Set / Get the vertical alignment.
  void SetVerticalAlignment(Alignment va);
  Alignment GetVerticalAlignment() const;

  // Set / Get the tooltip text.
  void set_tooltip_text(const base::string16& tooltip) {
    tooltip_text_ = tooltip;
  }
  const base::string16& tooltip_text() const { return tooltip_text_; }

  // Set / Get the accessible name text.
  void SetAccessibleName(const base::string16& name);
  const base::string16& GetAccessibleName() const;

  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override = 0;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override = 0;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  gfx::Size CalculatePreferredSize() const override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void PreferredSizeChanged() override;

 protected:
  // Returns the size the image will be painted.
  virtual gfx::Size GetImageSize() const = 0;

  // The requested image size.
  base::Optional<gfx::Size> image_size_;

 private:
  friend class ImageViewTest;

  // Recomputes and updates the |image_origin_|.
  void UpdateImageOrigin();

  // The origin of the image.
  gfx::Point image_origin_;

  // Horizontal alignment.
  Alignment horizontal_alignment_ = Alignment::CENTER;

  // Vertical alignment.
  Alignment vertical_alignment_ = Alignment::CENTER;

  // The current tooltip text.
  base::string16 tooltip_text_;

  // The current accessible name text.
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(ImageViewBase);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_IMAGE_VIEW_BASE_H_
