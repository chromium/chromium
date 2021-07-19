// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/image_view.h"

#include <utility>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/themed_vector_icon.h"
#include "ui/views/image_model_utils.h"

namespace views {

namespace {

// Returns the pixels for the bitmap in |image| at scale |image_scale|.
void* GetBitmapPixels(const gfx::ImageSkia& image, float image_scale) {
  DCHECK_NE(0.0f, image_scale);
  return image.GetRepresentation(image_scale).GetBitmap().getPixels();
}

}  // namespace

ImageView::ImageView() = default;

ImageView::ImageView(const ui::ImageModel& image_model) {
  SetImage(image_model);
}

ImageView::~ImageView() = default;

void ImageView::SetImage(const ui::ImageModel& image_model) {
  if (IsImageEqual(image_model))
    return;

  const gfx::Size pref_size = GetPreferredSize();
  image_model_ = image_model;
  scaled_image_ = gfx::ImageSkia();
  if (pref_size != GetPreferredSize())
    PreferredSizeChanged();
  SchedulePaint();
}

void ImageView::SetImageSize(const gfx::Size& image_size) {
  image_size_ = image_size;
  PreferredSizeChanged();
}

void ImageView::ResetImageSize() {
  image_size_.reset();
  PreferredSizeChanged();
}

gfx::Rect ImageView::GetImageBounds() const {
  return gfx::Rect(image_origin_, GetImageSize());
}

gfx::ImageSkia ImageView::GetImage() const {
  return views::GetImageSkiaFromImageModel(image_model_, GetNativeTheme());
}

ui::ImageModel ImageView::GetImageModel() const {
  return image_model_;
}

void ImageView::SetHorizontalAlignment(Alignment alignment) {
  if (alignment != horizontal_alignment_) {
    horizontal_alignment_ = alignment;
    UpdateImageOrigin();
    OnPropertyChanged(&horizontal_alignment_, kPropertyEffectsPaint);
  }
}

ImageView::Alignment ImageView::GetHorizontalAlignment() const {
  return horizontal_alignment_;
}

void ImageView::SetVerticalAlignment(Alignment alignment) {
  if (alignment != vertical_alignment_) {
    vertical_alignment_ = alignment;
    UpdateImageOrigin();
    OnPropertyChanged(&horizontal_alignment_, kPropertyEffectsPaint);
  }
}

ImageView::Alignment ImageView::GetVerticalAlignment() const {
  return vertical_alignment_;
}

void ImageView::SetAccessibleName(const std::u16string& accessible_name) {
  if (accessible_name_ == accessible_name)
    return;

  accessible_name_ = accessible_name;
  OnPropertyChanged(&accessible_name_, kPropertyEffectsNone);
  NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

const std::u16string& ImageView::GetAccessibleName() const {
  return accessible_name_.empty() ? tooltip_text_ : accessible_name_;
}

void ImageView::SetTooltipText(const std::u16string& tooltip) {
  tooltip_text_ = tooltip;
}

const std::u16string& ImageView::GetTooltipText() const {
  return tooltip_text_;
}

bool ImageView::IsImageEqual(const ui::ImageModel& image_model) const {
  if (image_model != image_model_)
    return false;

  // It's not feasible to run the old and new generators and compare output
  // here, so for safety, simply assume the new generator's output differs.
  if (image_model.IsImageGenerator())
    return false;

  if (!image_model.IsImage())
    return true;

  // An ImageModel's Image holds a handle to a backing store, which may have
  // changed since the last call to SetImage(). The expectation is that
  // SetImage() with different pixels is treated as though the image changed.
  // For this reason we compare not only the Image but also the pixels we last
  // painted.
  return last_paint_scale_ != 0.0f &&
         last_painted_bitmap_pixels_ ==
             GetBitmapPixels(image_model.GetImage().AsImageSkia(),
                             last_paint_scale_);
}

void ImageView::UpdateImageOrigin() {
  gfx::Size image_size = GetImageSize();
  gfx::Insets insets = GetInsets();

  int x = 0;
  // In order to properly handle alignment of images in RTL locales, we need
  // to flip the meaning of trailing and leading. For example, if the
  // horizontal alignment is set to trailing, then we'll use left alignment for
  // the image instead of right alignment if the UI layout is RTL.
  Alignment actual_horizontal_alignment = horizontal_alignment_;
  if (base::i18n::IsRTL() && (horizontal_alignment_ != Alignment::kCenter)) {
    actual_horizontal_alignment = (horizontal_alignment_ == Alignment::kLeading)
                                      ? Alignment::kTrailing
                                      : Alignment::kLeading;
  }
  switch (actual_horizontal_alignment) {
    case Alignment::kLeading:
      x = insets.left();
      break;
    case Alignment::kTrailing:
      x = width() - insets.right() - image_size.width();
      break;
    case Alignment::kCenter:
      x = (width() - insets.width() - image_size.width()) / 2 + insets.left();
      break;
  }

  int y = 0;
  switch (vertical_alignment_) {
    case Alignment::kLeading:
      y = insets.top();
      break;
    case Alignment::kTrailing:
      y = height() - insets.bottom() - image_size.height();
      break;
    case Alignment::kCenter:
      y = (height() - insets.height() - image_size.height()) / 2 + insets.top();
      break;
  }

  image_origin_ = gfx::Point(x, y);
}

gfx::Size ImageView::GetImageSize() const {
  return image_size_.value_or(image_model_.Size());
}

void ImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  OnPaintImage(canvas);
}

void ImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  const std::u16string& name = GetAccessibleName();
  if (name.empty()) {
    node_data->role = ax::mojom::Role::kNone;
    return;
  }

  node_data->role = ax::mojom::Role::kImage;
  node_data->SetName(name);
}

std::u16string ImageView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_text_;
}

gfx::Size ImageView::CalculatePreferredSize() const {
  gfx::Size size = GetImageSize();
  size.Enlarge(GetInsets().width(), GetInsets().height());
  return size;
}

views::PaintInfo::ScaleType ImageView::GetPaintScaleType() const {
  // ImageViewBase contains an image which is rastered at the device scale
  // factor. By default, the paint commands are recorded at a scale factor
  // slightly different from the device scale factor. Re-rastering the image at
  // this paint recording scale will result in a distorted image. Paint
  // recording scale might also not be uniform along the x & y axis, thus
  // resulting in further distortion in the aspect ratio of the final image.
  // |kUniformScaling| ensures that the paint recording scale is uniform along
  // the x & y axis and keeps the scale equal to the device scale factor.
  // See http://crbug.com/754010 for more details.
  return views::PaintInfo::ScaleType::kUniformScaling;
}

void ImageView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateImageOrigin();
}

void ImageView::PreferredSizeChanged() {
  View::PreferredSizeChanged();
  UpdateImageOrigin();
}

void ImageView::OnThemeChanged() {
  View::OnThemeChanged();
  if (image_model_.IsImageGenerator() ||
      (image_model_.IsVectorIcon() &&
       !image_model_.GetVectorIcon().has_color())) {
    scaled_image_ = gfx::ImageSkia();
    SchedulePaint();
  }
}

void ImageView::OnPaintImage(gfx::Canvas* canvas) {
  last_paint_scale_ = canvas->image_scale();
  last_painted_bitmap_pixels_ = nullptr;

  gfx::ImageSkia image = GetPaintImage(last_paint_scale_);
  if (image.isNull())
    return;

  gfx::Rect image_bounds(GetImageBounds());
  if (image_bounds.IsEmpty())
    return;

  if (image_bounds.size() != gfx::Size(image.width(), image.height())) {
    // Resize case
    cc::PaintFlags flags;
    flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
    canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                         image_bounds.x(), image_bounds.y(),
                         image_bounds.width(), image_bounds.height(), true,
                         flags);
  } else {
    canvas->DrawImageInt(image, image_bounds.x(), image_bounds.y());
  }
  last_painted_bitmap_pixels_ = GetBitmapPixels(image, last_paint_scale_);
}

gfx::ImageSkia ImageView::GetPaintImage(float scale) {
  if (image_model_.IsEmpty())
    return gfx::ImageSkia();

  if (image_model_.IsImage() || image_model_.IsImageGenerator()) {
    const gfx::ImageSkia image =
        views::GetImageSkiaFromImageModel(image_model_, GetNativeTheme());
    if (image.isNull())
      return image;

    const gfx::ImageSkiaRep& rep = image.GetRepresentation(scale);
    if (rep.scale() == scale)
      return image;

    if (scaled_image_.HasRepresentation(scale))
      return scaled_image_;

    // Only caches one image rep for the current scale.
    scaled_image_ = gfx::ImageSkia();

    gfx::Size scaled_size =
        gfx::ScaleToCeiledSize(rep.pixel_size(), scale / rep.scale());
    scaled_image_.AddRepresentation(gfx::ImageSkiaRep(
        skia::ImageOperations::Resize(
            rep.GetBitmap(), skia::ImageOperations::RESIZE_BEST,
            scaled_size.width(), scaled_size.height()),
        scale));
  } else if (scaled_image_.isNull()) {
    scaled_image_ =
        views::GetImageSkiaFromImageModel(image_model_, GetNativeTheme());
  }
  return scaled_image_;
}

BEGIN_METADATA(ImageView, View)
ADD_PROPERTY_METADATA(Alignment, HorizontalAlignment)
ADD_PROPERTY_METADATA(Alignment, VerticalAlignment)
ADD_PROPERTY_METADATA(std::u16string, AccessibleName)
ADD_PROPERTY_METADATA(std::u16string, TooltipText)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(views::ImageView::Alignment,
                       {views::ImageView::Alignment::kLeading, u"kLeading"},
                       {views::ImageView::Alignment::kCenter, u"kCenter"},
                       {views::ImageView::Alignment::kTrailing, u"kTrailing"})
