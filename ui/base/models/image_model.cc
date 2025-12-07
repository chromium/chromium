// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/image_model.h"

#include <tuple>
#include <variant>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_utils.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/base/themed_vector_icon.h"
#endif

namespace ui {

VectorIconModel::VectorIconModel() = default;

VectorIconModel::VectorIconModel(const gfx::VectorIcon& vector_icon,
                                 ui::ColorVariant color,
                                 int icon_size,
                                 const gfx::VectorIcon* badge_icon)
    : vector_icon_(&vector_icon),
      icon_size_(icon_size),
      color_(color),
      badge_icon_(badge_icon) {}

VectorIconModel::~VectorIconModel() = default;

VectorIconModel::VectorIconModel(const VectorIconModel&) = default;

VectorIconModel& VectorIconModel::operator=(const VectorIconModel&) = default;

VectorIconModel::VectorIconModel(VectorIconModel&&) = default;

VectorIconModel& VectorIconModel::operator=(VectorIconModel&&) = default;

ImageModel::ImageModel() = default;

ImageModel::~ImageModel() = default;

ImageModel::ImageModel(const ImageModel&) = default;

ImageModel& ImageModel::operator=(const ImageModel&) = default;

ImageModel::ImageModel(ImageModel&&) = default;

ImageModel& ImageModel::operator=(ImageModel&&) = default;

// static
ImageModel ImageModel::FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                      ui::ColorVariant color,
                                      int icon_size,
                                      const gfx::VectorIcon* badge_icon) {
  if (!icon_size)
    icon_size = gfx::GetDefaultSizeOfVectorIcon(vector_icon);
  return ImageModel(VectorIconModel(vector_icon, color, icon_size, badge_icon));
}

// static
ImageModel ImageModel::FromImage(const gfx::Image& image) {
  return ImageModel(image);
}

// static
ImageModel ImageModel::FromImageSkia(const gfx::ImageSkia& image_skia) {
  return ImageModel(image_skia);
}

// static
ImageModel ImageModel::FromResourceId(int resource_id) {
  return ImageModel::FromImage(
      ResourceBundle::GetSharedInstance().GetImageNamed(resource_id));
}

// static
ImageModel ImageModel::FromImageGenerator(ImageGenerator generator,
                                          gfx::Size size) {
  return ImageModel(ImageGeneratorAndSize(generator, size));
}

bool ImageModel::IsEmpty() const {
  return !IsVectorIcon() && !IsImage() && !IsImageGenerator();
}

bool ImageModel::IsVectorIcon() const {
  return std::holds_alternative<VectorIconModel>(icon_) &&
         !std::get<VectorIconModel>(icon_).is_empty();
}

bool ImageModel::IsImage() const {
  return std::holds_alternative<gfx::Image>(icon_) &&
         !std::get<gfx::Image>(icon_).IsEmpty();
}

bool ImageModel::IsImageGenerator() const {
  return std::holds_alternative<ImageGeneratorAndSize>(icon_) &&
         !std::get<ImageGeneratorAndSize>(icon_).size.IsEmpty();
}

gfx::Size ImageModel::Size() const {
  if (IsVectorIcon()) {
    const int icon_size = GetVectorIcon().icon_size_;
    return gfx::Size(icon_size, icon_size);
  }
  if (IsImage())
    return GetImage().Size();
  return IsImageGenerator() ? std::get<ImageGeneratorAndSize>(icon_).size
                            : gfx::Size();
}

VectorIconModel ImageModel::GetVectorIcon() const {
  DCHECK(IsVectorIcon());
  return std::get<VectorIconModel>(icon_);
}

gfx::Image ImageModel::GetImage() const {
  DCHECK(IsImage());
  return std::get<gfx::Image>(icon_);
}

ImageModel::ImageGenerator ImageModel::GetImageGenerator() const {
  DCHECK(IsImageGenerator());
  return std::get<ImageGeneratorAndSize>(icon_).generator;
}

gfx::ImageSkia ImageModel::Rasterize(
    const ui::ColorProvider* color_provider) const {
  TRACE_EVENT0("ui", "ImageModel::Rasterize");
  if (IsImage())
    return GetImage().AsImageSkia();

  if (IsVectorIcon()) {
#if BUILDFLAG(IS_IOS)
    NOTREACHED();
#else
    DCHECK(color_provider);
    return ThemedVectorIcon(GetVectorIcon()).GetImageSkia(color_provider);
#endif
  }

  if (IsImageGenerator())
    return GetImageGenerator().Run(color_provider);

  return gfx::ImageSkia();
}

ImageModel::ImageGeneratorAndSize::ImageGeneratorAndSize(
    ImageGenerator generator,
    gfx::Size size)
    : generator(std::move(generator)), size(std::move(size)) {}

ImageModel::ImageGeneratorAndSize::ImageGeneratorAndSize(
    const ImageGeneratorAndSize&) = default;

ImageModel::ImageGeneratorAndSize& ImageModel::ImageGeneratorAndSize::operator=(
    const ImageGeneratorAndSize&) = default;

ImageModel::ImageGeneratorAndSize::~ImageGeneratorAndSize() = default;

ImageModel::ImageModel(const VectorIconModel& vector_icon_model)
    : icon_(vector_icon_model) {}

ImageModel::ImageModel(const gfx::Image& image) : icon_(image) {}

ImageModel::ImageModel(const gfx::ImageSkia& image_skia)
    : ImageModel(gfx::Image(image_skia)) {}

ImageModel::ImageModel(ImageGeneratorAndSize image_generator)
    : icon_(std::move(image_generator)) {}

}  // namespace ui
