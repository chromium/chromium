// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_IMAGE_VIEW_BASE_H_
#define UI_VIEWS_CONTROLS_IMAGE_VIEW_BASE_H_

#include <optional>
#include <string>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {

class VIEWS_EXPORT ImageViewBase : public View {
  METADATA_HEADER(ImageViewBase, View)

 public:
  enum class Alignment { kLeading, kCenter, kTrailing };

  ImageViewBase();
  ImageViewBase(const ImageViewBase&) = delete;
  ImageViewBase& operator=(const ImageViewBase&) = delete;
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

  // Set the tooltip text.
  virtual void SetTooltipText(const std::u16string& tooltip);
  const std::u16string& GetTooltipText() const;

  // Overridden from View:
  void AdjustAccessibleName(std::u16string& new_name,
                            ax::mojom::NameFrom& name_from) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override;
  views::PaintInfo::ScaleType GetPaintScaleType() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void PreferredSizeChanged() override;

 protected:
  // Returns the size the image will be painted.
  virtual gfx::Size GetImageSize() const = 0;

  // The requested image size.
  std::optional<gfx::Size> image_size_;

 private:
  friend class ImageViewTest;

  // Recomputes and updates the |image_origin_|.
  void UpdateImageOrigin();

  // The origin of the image.
  gfx::Point image_origin_;

  // Horizontal alignment.
  Alignment horizontal_alignment_ = Alignment::kCenter;

  // Vertical alignment.
  Alignment vertical_alignment_ = Alignment::kCenter;

  // The current tooltip text.
  std::u16string tooltip_text_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, ImageViewBase, View)
VIEW_BUILDER_PROPERTY(gfx::Size, ImageSize)
VIEW_BUILDER_PROPERTY(ImageViewBase::Alignment, HorizontalAlignment)
VIEW_BUILDER_PROPERTY(ImageViewBase::Alignment, VerticalAlignment)
VIEW_BUILDER_PROPERTY(std::u16string, TooltipText)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, ImageViewBase)

#endif  // UI_VIEWS_CONTROLS_IMAGE_VIEW_BASE_H_
