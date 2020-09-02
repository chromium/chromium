// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_IMAGE_MODEL_H_
#define UI_BASE_MODELS_IMAGE_MODEL_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {

// The following classes encapsulate the various ways that a model may provide
// or otherwise specify an icon or image. Most notably, these are used by the
// MenuModel and SimpleMenuModel for building actual menus.
//
// The VectorIconModel represents the combination of the icon path and its
// optional color id. The optional color is provided by the color id which is
// eventually resolved by the ColorProvider from the correct context. This class
// is only used internal to ImageModel and should never be instantiated except
// by ImageModel.

class COMPONENT_EXPORT(UI_BASE) VectorIconModel {
 public:
  VectorIconModel();
  VectorIconModel(const VectorIconModel&);
  VectorIconModel& operator=(const VectorIconModel&);
  VectorIconModel(VectorIconModel&&);
  VectorIconModel& operator=(VectorIconModel&&);
  ~VectorIconModel();

  bool is_empty() const { return !vector_icon_; }

  bool operator==(const VectorIconModel& other) const;
  bool operator!=(const VectorIconModel& other) const;

 private:
  friend class ThemedVectorIcon;
  friend class ImageModel;

  VectorIconModel(const gfx::VectorIcon& vector_icon,
                  int color_id,
                  int icon_size);
  // TODO (kylixrd): This should be eventually removed once all instances of
  // hard-coded SkColor constants are removed in favor of using a color id.
  VectorIconModel(const gfx::VectorIcon& vector_icon,
                  SkColor color,
                  int icon_size);

  const gfx::VectorIcon* vector_icon() const { return vector_icon_; }
  int icon_size() const { return icon_size_; }
  int color_id() const { return absl::get<int>(color_); }
  SkColor color() const { return absl::get<SkColor>(color_); }
  bool has_color() const { return absl::holds_alternative<SkColor>(color_); }

  const gfx::VectorIcon* vector_icon_ = nullptr;
  int icon_size_ = 0;
  absl::variant<int, SkColor> color_ = gfx::kPlaceholderColor;
};

// ImageModel encapsulates either a gfx::Image or a VectorIconModel. Only one
// of the two may be specified at a given time. This class is instantiated via
// the FromXXXX static factory functions.

class COMPONENT_EXPORT(UI_BASE) ImageModel {
 public:
  ImageModel();
  ImageModel(const ImageModel&);
  ImageModel& operator=(const ImageModel&);
  ImageModel(ImageModel&&);
  ImageModel& operator=(ImageModel&&);
  ~ImageModel();

  static ImageModel FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                   int color_id = -1,
                                   int icon_size = 0);
  static ImageModel FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                   SkColor color,
                                   int icon_size = 0);
  static ImageModel FromImage(const gfx::Image& image);
  static ImageModel FromImageSkia(const gfx::ImageSkia& image_skia);
  static ImageModel FromResourceId(int resource_id);

  bool IsEmpty() const;
  bool IsVectorIcon() const;
  bool IsImage() const;
  gfx::Size Size() const;
  // Only valid if IsVectorIcon() or IsImage() return true, respectively.
  VectorIconModel GetVectorIcon() const;
  gfx::Image GetImage() const;

  // Checks if both model yield equal images.
  bool operator==(const ImageModel& other) const;
  bool operator!=(const ImageModel& other) const;

 private:
  ImageModel(const gfx::Image& image);
  ImageModel(const gfx::ImageSkia& image_skia);
  ImageModel(const VectorIconModel& vector_icon_model);

  absl::variant<VectorIconModel, gfx::Image> icon_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_IMAGE_MODEL_H_
