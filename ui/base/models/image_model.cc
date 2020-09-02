// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace ui {

VectorIconModel::VectorIconModel() = default;

VectorIconModel::VectorIconModel(const gfx::VectorIcon& vector_icon,
                                 int color_id,
                                 int icon_size)
    : vector_icon_(&vector_icon), icon_size_(icon_size), color_(color_id) {}

VectorIconModel::VectorIconModel(const gfx::VectorIcon& vector_icon,
                                 SkColor color,
                                 int icon_size)
    : vector_icon_(&vector_icon), icon_size_(icon_size), color_(color) {}

VectorIconModel::~VectorIconModel() = default;

VectorIconModel::VectorIconModel(const VectorIconModel&) = default;

VectorIconModel& VectorIconModel::operator=(const VectorIconModel&) = default;

VectorIconModel::VectorIconModel(VectorIconModel&&) = default;

VectorIconModel& VectorIconModel::operator=(VectorIconModel&&) = default;

bool VectorIconModel::operator==(const VectorIconModel& other) const {
  return std::tie(vector_icon_, icon_size_, color_) ==
         std::tie(other.vector_icon_, other.icon_size_, other.color_);
}

bool VectorIconModel::operator!=(const VectorIconModel& other) const {
  return !(*this == other);
}

ImageModel::ImageModel() = default;

ImageModel::ImageModel(const VectorIconModel& vector_icon_model)
    : icon_(vector_icon_model) {}

ImageModel::ImageModel(const gfx::Image& image) : icon_(image) {}

ImageModel::ImageModel(const gfx::ImageSkia& image_skia)
    : ImageModel(gfx::Image(image_skia)) {}

ImageModel::~ImageModel() = default;

ImageModel::ImageModel(const ImageModel&) = default;

ImageModel& ImageModel::operator=(const ImageModel&) = default;

ImageModel::ImageModel(ImageModel&&) = default;

ImageModel& ImageModel::operator=(ImageModel&&) = default;

// static
ImageModel ImageModel::FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                      int color_id,
                                      int icon_size) {
  return ImageModel(VectorIconModel(vector_icon, color_id, icon_size));
}

// static
ImageModel ImageModel::FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                      SkColor color,
                                      int icon_size) {
  return ImageModel(VectorIconModel(vector_icon, color, icon_size));
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

bool ImageModel::IsEmpty() const {
  return !IsVectorIcon() && !IsImage();
}

bool ImageModel::IsVectorIcon() const {
  return absl::holds_alternative<VectorIconModel>(icon_) &&
         !absl::get<VectorIconModel>(icon_).is_empty();
}

bool ImageModel::IsImage() const {
  return absl::holds_alternative<gfx::Image>(icon_) &&
         !absl::get<gfx::Image>(icon_).IsEmpty();
}

gfx::Size ImageModel::Size() const {
  if (IsVectorIcon()) {
    const int icon_size = GetVectorIcon().icon_size();
    return gfx::Size(icon_size, icon_size);
  }
  return IsImage() ? GetImage().Size() : gfx::Size();
}

VectorIconModel ImageModel::GetVectorIcon() const {
  DCHECK(IsVectorIcon());
  return absl::get<VectorIconModel>(icon_);
}

gfx::Image ImageModel::GetImage() const {
  DCHECK(IsImage());
  return absl::get<gfx::Image>(icon_);
}

bool ImageModel::operator==(const ImageModel& other) const {
  return icon_ == other.icon_;
}

bool ImageModel::operator!=(const ImageModel& other) const {
  return !(*this == other);
}

}  // namespace ui
