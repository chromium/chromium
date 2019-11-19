// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_operations.h"

#include <stddef.h>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/skia_util.h"

namespace gfx {
namespace {

gfx::Size DIPToPixelSize(gfx::Size dip_size, float scale) {
  return ScaleToCeiledSize(dip_size, scale);
}

gfx::Rect DIPToPixelBounds(gfx::Rect dip_bounds, float scale) {
  return gfx::Rect(ScaleToFlooredPoint(dip_bounds.origin(), scale),
                   DIPToPixelSize(dip_bounds.size(), scale));
}

// Returns an image rep for the ImageSkiaSource to return to visually indicate
// an error.
ImageSkiaRep GetErrorImageRep(float scale, const gfx::Size& pixel_size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(pixel_size.width(), pixel_size.height());
  bitmap.eraseColor(SK_ColorRED);
  return gfx::ImageSkiaRep(bitmap, scale);
}

// A base image source class that creates an image from two source images.
// This class guarantees that two ImageSkiaReps have have the same pixel size.
class BinaryImageSource : public gfx::ImageSkiaSource {
 protected:
  BinaryImageSource(const ImageSkia& first,
                    const ImageSkia& second,
                    const char* source_name)
      : first_(first),
        second_(second),
        source_name_(source_name) {
  }
  ~BinaryImageSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep first_rep = first_.GetRepresentation(scale);
    ImageSkiaRep second_rep = second_.GetRepresentation(scale);
    if (first_rep.pixel_size() != second_rep.pixel_size()) {
      DCHECK_NE(first_rep.scale(), second_rep.scale());
      if (first_rep.scale() == second_rep.scale()) {
        LOG(ERROR) << "ImageSkiaRep size mismatch in " << source_name_;
        return GetErrorImageRep(first_rep.scale(),first_rep.pixel_size());
      }
      first_rep = first_.GetRepresentation(1.0f);
      second_rep = second_.GetRepresentation(1.0f);
      DCHECK_EQ(first_rep.pixel_width(), second_rep.pixel_width());
      DCHECK_EQ(first_rep.pixel_height(), second_rep.pixel_height());
      if (first_rep.pixel_size() != second_rep.pixel_size()) {
        LOG(ERROR) << "ImageSkiaRep size mismatch in " << source_name_;
        return GetErrorImageRep(first_rep.scale(), first_rep.pixel_size());
      }
    } else {
      DCHECK_EQ(first_rep.scale(), second_rep.scale());
    }
    return CreateImageSkiaRep(first_rep, second_rep);
  }

  // Creates a final image from two ImageSkiaReps. The pixel size of
  // the two images are guaranteed to be the same.
  virtual ImageSkiaRep CreateImageSkiaRep(
      const ImageSkiaRep& first_rep,
      const ImageSkiaRep& second_rep) const = 0;

 private:
  const ImageSkia first_;
  const ImageSkia second_;
  // The name of a class that implements the BinaryImageSource.
  // The subclass is responsible for managing the memory.
  const char* source_name_;

  DISALLOW_COPY_AND_ASSIGN(BinaryImageSource);
};

class BlendingImageSource : public BinaryImageSource {
 public:
  BlendingImageSource(const ImageSkia& first,
                      const ImageSkia& second,
                      double alpha)
      : BinaryImageSource(first, second, "BlendingImageSource"),
        alpha_(alpha) {
  }

  ~BlendingImageSource() override {}

  // BinaryImageSource overrides:
  ImageSkiaRep CreateImageSkiaRep(
      const ImageSkiaRep& first_rep,
      const ImageSkiaRep& second_rep) const override {
    SkBitmap blended = SkBitmapOperations::CreateBlendedBitmap(
        first_rep.GetBitmap(), second_rep.GetBitmap(), alpha_);
    return ImageSkiaRep(blended, first_rep.scale());
  }

 private:
  double alpha_;

  DISALLOW_COPY_AND_ASSIGN(BlendingImageSource);
};

class SuperimposedImageSource : public gfx::CanvasImageSource {
 public:
  SuperimposedImageSource(const ImageSkia& first, const ImageSkia& second)
      : gfx::CanvasImageSource(first.size()), first_(first), second_(second) {}

  ~SuperimposedImageSource() override {}

  // gfx::CanvasImageSource override.
  void Draw(Canvas* canvas) override {
    canvas->DrawImageInt(first_, 0, 0);
    canvas->DrawImageInt(second_,
                         (first_.width() - second_.width()) / 2,
                         (first_.height() - second_.height()) / 2);
  }

 private:
  const ImageSkia first_;
  const ImageSkia second_;

  DISALLOW_COPY_AND_ASSIGN(SuperimposedImageSource);
};

class TransparentImageSource : public gfx::ImageSkiaSource {
 public:
  TransparentImageSource(const ImageSkia& image, double alpha)
      : image_(image),
        alpha_(alpha) {
  }

  ~TransparentImageSource() override {}

 private:
  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep image_rep = image_.GetRepresentation(scale);
    SkBitmap alpha;
    alpha.allocN32Pixels(image_rep.pixel_width(),
                         image_rep.pixel_height());
    alpha.eraseColor(SkColorSetARGB(alpha_ * 255, 0, 0, 0));
    return ImageSkiaRep(
        SkBitmapOperations::CreateMaskedBitmap(image_rep.GetBitmap(), alpha),
        image_rep.scale());
  }

  ImageSkia image_;
  double alpha_;

  DISALLOW_COPY_AND_ASSIGN(TransparentImageSource);
};

class MaskedImageSource : public BinaryImageSource {
 public:
  MaskedImageSource(const ImageSkia& rgb, const ImageSkia& alpha)
      : BinaryImageSource(rgb, alpha, "MaskedImageSource") {
  }

  ~MaskedImageSource() override {}

  // BinaryImageSource overrides:
  ImageSkiaRep CreateImageSkiaRep(
      const ImageSkiaRep& first_rep,
      const ImageSkiaRep& second_rep) const override {
    return ImageSkiaRep(SkBitmapOperations::CreateMaskedBitmap(
                            first_rep.GetBitmap(), second_rep.GetBitmap()),
                        first_rep.scale());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaskedImageSource);
};

class TiledImageSource : public gfx::ImageSkiaSource {
 public:
  TiledImageSource(const ImageSkia& source,
                   int src_x, int src_y,
                   int dst_w, int dst_h)
      : source_(source),
        src_x_(src_x),
        src_y_(src_y),
        dst_w_(dst_w),
        dst_h_(dst_h) {
  }

  ~TiledImageSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep source_rep = source_.GetRepresentation(scale);
    gfx::Rect bounds = DIPToPixelBounds(gfx::Rect(src_x_, src_y_, dst_w_,
                                                  dst_h_), source_rep.scale());
    return ImageSkiaRep(SkBitmapOperations::CreateTiledBitmap(
                            source_rep.GetBitmap(), bounds.x(), bounds.y(),
                            bounds.width(), bounds.height()),
                        source_rep.scale());
  }

 private:
  const ImageSkia source_;
  const int src_x_;
  const int src_y_;
  const int dst_w_;
  const int dst_h_;

  DISALLOW_COPY_AND_ASSIGN(TiledImageSource);
};

class HSLImageSource : public gfx::ImageSkiaSource {
 public:
  HSLImageSource(const ImageSkia& image,
                 const color_utils::HSL& hsl_shift)
      : image_(image),
        hsl_shift_(hsl_shift) {
  }

  ~HSLImageSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep image_rep = image_.GetRepresentation(scale);
    return gfx::ImageSkiaRep(SkBitmapOperations::CreateHSLShiftedBitmap(
                                 image_rep.GetBitmap(), hsl_shift_),
                             image_rep.scale());
  }

 private:
  const gfx::ImageSkia image_;
  const color_utils::HSL hsl_shift_;
  DISALLOW_COPY_AND_ASSIGN(HSLImageSource);
};

// ImageSkiaSource which uses SkBitmapOperations::CreateButtonBackground
// to generate image reps for the target image.  The image and mask can be
// diferent sizes (crbug.com/171725).
class ButtonImageSource: public gfx::ImageSkiaSource {
 public:
  ButtonImageSource(SkColor color,
                    const ImageSkia& image,
                    const ImageSkia& mask)
      : color_(color),
        image_(image),
        mask_(mask) {
  }

  ~ButtonImageSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep image_rep = image_.GetRepresentation(scale);
    ImageSkiaRep mask_rep = mask_.GetRepresentation(scale);
    if (image_rep.scale() != mask_rep.scale()) {
      image_rep = image_.GetRepresentation(1.0f);
      mask_rep = mask_.GetRepresentation(1.0f);
    }
    return gfx::ImageSkiaRep(
        SkBitmapOperations::CreateButtonBackground(
            color_, image_rep.GetBitmap(), mask_rep.GetBitmap()),
        image_rep.scale());
  }

 private:
  const SkColor color_;
  const ImageSkia image_;
  const ImageSkia mask_;

  DISALLOW_COPY_AND_ASSIGN(ButtonImageSource);
};

// ImageSkiaSource which uses SkBitmap::extractSubset to generate image reps
// for the target image.
class ExtractSubsetImageSource: public gfx::ImageSkiaSource {
 public:
  ExtractSubsetImageSource(const gfx::ImageSkia& image,
                           const gfx::Rect& subset_bounds)
      : image_(image),
        subset_bounds_(subset_bounds) {
  }

  ~ExtractSubsetImageSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep image_rep = image_.GetRepresentation(scale);
    SkIRect subset_bounds_in_pixel = RectToSkIRect(
        DIPToPixelBounds(subset_bounds_, image_rep.scale()));
    SkBitmap dst;
    bool success =
        image_rep.GetBitmap().extractSubset(&dst, subset_bounds_in_pixel);
    DCHECK(success);
    return gfx::ImageSkiaRep(dst, image_rep.scale());
  }

 private:
  const gfx::ImageSkia image_;
  const gfx::Rect subset_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ExtractSubsetImageSource);
};

// ResizeSource resizes relevant image reps in |source| to |target_dip_size|
// for requested scale factors.
class ResizeSource : public ImageSkiaSource {
 public:
  ResizeSource(const ImageSkia& source,
               skia::ImageOperations::ResizeMethod method,
               const Size& target_dip_size)
      : source_(source),
        resize_method_(method),
        target_dip_size_(target_dip_size) {
  }
  ~ResizeSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    const ImageSkiaRep& image_rep = source_.GetRepresentation(scale);
    if (image_rep.GetWidth() == target_dip_size_.width() &&
        image_rep.GetHeight() == target_dip_size_.height())
      return image_rep;

    const Size target_pixel_size = DIPToPixelSize(target_dip_size_, scale);
    const SkBitmap resized = skia::ImageOperations::Resize(
        image_rep.GetBitmap(), resize_method_, target_pixel_size.width(),
        target_pixel_size.height());
    return ImageSkiaRep(resized, scale);
  }

 private:
  const ImageSkia source_;
  skia::ImageOperations::ResizeMethod resize_method_;
  const Size target_dip_size_;

  DISALLOW_COPY_AND_ASSIGN(ResizeSource);
};

// DropShadowSource generates image reps with drop shadow for image reps in
// |source| that represent requested scale factors.
class DropShadowSource : public ImageSkiaSource {
 public:
  DropShadowSource(const ImageSkia& source, const ShadowValues& shadows_in_dip)
      : source_(source), shadows_in_dip_(shadows_in_dip) {}
  ~DropShadowSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    const ImageSkiaRep& image_rep = source_.GetRepresentation(scale);

    ShadowValues shadows_in_pixel;
    for (size_t i = 0; i < shadows_in_dip_.size(); ++i)
      shadows_in_pixel.push_back(shadows_in_dip_[i].Scale(scale));

    const SkBitmap shadow_bitmap = SkBitmapOperations::CreateDropShadow(
        image_rep.GetBitmap(), shadows_in_pixel);
    return ImageSkiaRep(shadow_bitmap, image_rep.scale());
  }

 private:
  const ImageSkia source_;
  const ShadowValues shadows_in_dip_;

  DISALLOW_COPY_AND_ASSIGN(DropShadowSource);
};

// An image source that is 1px wide, suitable for tiling horizontally.
class HorizontalShadowSource : public CanvasImageSource {
 public:
  HorizontalShadowSource(const std::vector<ShadowValue>& shadows,
                         bool fades_down)
      : CanvasImageSource(Size(1, GetHeightForShadows(shadows))),
        shadows_(shadows),
        fades_down_(fades_down) {}
  ~HorizontalShadowSource() override {}

  // CanvasImageSource overrides:
  void Draw(Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setLooper(CreateShadowDrawLooper(shadows_));
    canvas->DrawRect(RectF(0, fades_down_ ? -1 : size().height(), 1, 1), flags);
  }

 private:
  static int GetHeightForShadows(const std::vector<ShadowValue>& shadows) {
    int height = 0;
    for (const auto& shadow : shadows) {
      height = std::max(height, shadow.y() + ToCeiledInt(shadow.blur() / 2));
    }
    return height;
  }

  const std::vector<ShadowValue> shadows_;

  // The orientation of the shadow (true for shadows that emanate downwards).
  bool fades_down_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalShadowSource);
};

// RotatedSource generates image reps that are rotations of those in
// |source| that represent requested scale factors.
class RotatedSource : public ImageSkiaSource {
 public:
  RotatedSource(const ImageSkia& source,
                SkBitmapOperations::RotationAmount rotation)
    : source_(source),
      rotation_(rotation) {
  }
  ~RotatedSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    const ImageSkiaRep& image_rep = source_.GetRepresentation(scale);
    const SkBitmap rotated_bitmap =
        SkBitmapOperations::Rotate(image_rep.GetBitmap(), rotation_);
    return ImageSkiaRep(rotated_bitmap, image_rep.scale());
  }

 private:
  const ImageSkia source_;
  const SkBitmapOperations::RotationAmount rotation_;

  DISALLOW_COPY_AND_ASSIGN(RotatedSource);
};

class IconWithBadgeSource : public gfx::CanvasImageSource {
 public:
  IconWithBadgeSource(const ImageSkia& icon, const ImageSkia& badge)
      : gfx::CanvasImageSource(icon.size()), icon_(icon), badge_(badge) {}

  ~IconWithBadgeSource() override {}

  // gfx::CanvasImageSource override.
  void Draw(Canvas* canvas) override {
    canvas->DrawImageInt(icon_, 0, 0);
    canvas->DrawImageInt(badge_, (icon_.width() - badge_.width()),
                         (icon_.height() - badge_.height()));
  }

 private:
  const ImageSkia icon_;
  const ImageSkia badge_;

  DISALLOW_COPY_AND_ASSIGN(IconWithBadgeSource);
};

// ImageSkiaSource which uses SkBitmapOperations::CreateColorMask
// to generate image reps for the target image.
class ColorMaskSource : public gfx::ImageSkiaSource {
 public:
  ColorMaskSource(const ImageSkia& image, SkColor color)
      : image_(image), color_(color) {}

  ~ColorMaskSource() override {}

  // gfx::ImageSkiaSource overrides:
  ImageSkiaRep GetImageForScale(float scale) override {
    ImageSkiaRep image_rep = image_.GetRepresentation(scale);
    return ImageSkiaRep(
        SkBitmapOperations::CreateColorMask(image_rep.GetBitmap(), color_),
        image_rep.scale());
  }

 private:
  const ImageSkia image_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(ColorMaskSource);
};

}  // namespace

// static
ImageSkia ImageSkiaOperations::CreateBlendedImage(const ImageSkia& first,
                                                  const ImageSkia& second,
                                                  double alpha) {
  if (first.isNull() || second.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<BlendingImageSource>(first, second, alpha),
                   first.size());
}

// static
ImageSkia ImageSkiaOperations::CreateSuperimposedImage(
    const ImageSkia& first,
    const ImageSkia& second) {
  if (first.isNull() || second.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<SuperimposedImageSource>(first, second),
                   first.size());
}

// static
ImageSkia ImageSkiaOperations::CreateTransparentImage(const ImageSkia& image,
                                                      double alpha) {
  if (image.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<TransparentImageSource>(image, alpha),
                   image.size());
}

// static
ImageSkia ImageSkiaOperations::CreateMaskedImage(const ImageSkia& rgb,
                                                 const ImageSkia& alpha) {
  if (rgb.isNull() || alpha.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<MaskedImageSource>(rgb, alpha), rgb.size());
}

// static
ImageSkia ImageSkiaOperations::CreateTiledImage(const ImageSkia& source,
                                                int src_x, int src_y,
                                                int dst_w, int dst_h) {
  if (source.isNull())
    return ImageSkia();

  return ImageSkia(
      std::make_unique<TiledImageSource>(source, src_x, src_y, dst_w, dst_h),
      gfx::Size(dst_w, dst_h));
}

// static
ImageSkia ImageSkiaOperations::CreateHSLShiftedImage(
    const ImageSkia& image,
    const color_utils::HSL& hsl_shift) {
  if (image.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<HSLImageSource>(image, hsl_shift),
                   image.size());
}

// static
ImageSkia ImageSkiaOperations::CreateButtonBackground(SkColor color,
                                                      const ImageSkia& image,
                                                      const ImageSkia& mask) {
  if (image.isNull() || mask.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<ButtonImageSource>(color, image, mask),
                   mask.size());
}

// static
ImageSkia ImageSkiaOperations::ExtractSubset(const ImageSkia& image,
                                             const Rect& subset_bounds) {
  gfx::Rect clipped_bounds =
      gfx::IntersectRects(subset_bounds, gfx::Rect(image.size()));
  if (image.isNull() || clipped_bounds.IsEmpty())
    return ImageSkia();
  if (clipped_bounds == gfx::Rect(image.size()))
    return image;

  return ImageSkia(
      std::make_unique<ExtractSubsetImageSource>(image, clipped_bounds),
      clipped_bounds.size());
}

// static
ImageSkia ImageSkiaOperations::CreateResizedImage(
    const ImageSkia& source,
    skia::ImageOperations::ResizeMethod method,
    const Size& target_dip_size) {
  if (source.isNull())
    return ImageSkia();
  if (source.size() == target_dip_size)
    return source;

  return ImageSkia(
      std::make_unique<ResizeSource>(source, method, target_dip_size),
      target_dip_size);
}

// static
ImageSkia ImageSkiaOperations::CreateImageWithDropShadow(
    const ImageSkia& source,
    const ShadowValues& shadows) {
  if (source.isNull())
    return ImageSkia();

  const gfx::Insets shadow_padding = -gfx::ShadowValue::GetMargin(shadows);
  gfx::Size shadow_image_size = source.size();
  shadow_image_size.Enlarge(shadow_padding.width(),
                            shadow_padding.height());
  return ImageSkia(std::make_unique<DropShadowSource>(source, shadows),
                   shadow_image_size);
}

// static
ImageSkia ImageSkiaOperations::CreateHorizontalShadow(
    const std::vector<ShadowValue>& shadows,
    bool fades_down) {
  auto* source = new HorizontalShadowSource(shadows, fades_down);
  return ImageSkia(base::WrapUnique(source), source->size());
}

// static
ImageSkia ImageSkiaOperations::CreateRotatedImage(
      const ImageSkia& source,
      SkBitmapOperations::RotationAmount rotation) {
  if (source.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<RotatedSource>(source, rotation),
                   SkBitmapOperations::ROTATION_180_CW == rotation
                       ? source.size()
                       : gfx::Size(source.height(), source.width()));
}

// static
ImageSkia ImageSkiaOperations::CreateIconWithBadge(const ImageSkia& icon,
                                                   const ImageSkia& badge) {
  if (icon.isNull())
    return ImageSkia();

  if (badge.isNull())
    return icon;

  return ImageSkia(std::make_unique<IconWithBadgeSource>(icon, badge),
                   icon.size());
}

// static
ImageSkia ImageSkiaOperations::CreateColorMask(const ImageSkia& image,
                                               SkColor color) {
  if (image.isNull())
    return ImageSkia();

  return ImageSkia(std::make_unique<ColorMaskSource>(image, color),
                   image.size());
}
}  // namespace gfx
