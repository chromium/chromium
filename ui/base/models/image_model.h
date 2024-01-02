// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_IMAGE_MODEL_H_
#define UI_BASE_MODELS_IMAGE_MODEL_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ui {

class ColorProvider;

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

  const gfx::VectorIcon* vector_icon() const { return vector_icon_; }
  int icon_size() const { return icon_size_; }
  ColorId color_id() const { return absl::get<ColorId>(color_); }
  SkColor color() const { return absl::get<SkColor>(color_); }
  bool has_color() const { return absl::holds_alternative<SkColor>(color_); }
  const gfx::VectorIcon* badge_icon() const { return badge_icon_; }

 private:
  friend class ImageModel;

  VectorIconModel(const gfx::VectorIcon& vector_icon,
                  ColorId color_id,
                  int icon_size,
                  const gfx::VectorIcon* badge_icon);
  // TODO (kylixrd): This should be eventually removed once all instances of
  // hard-coded SkColor constants are removed in favor of using a color id.
  VectorIconModel(const gfx::VectorIcon& vector_icon,
                  SkColor color,
                  int icon_size,
                  const gfx::VectorIcon* badge_icon);

  raw_ptr<const gfx::VectorIcon> vector_icon_ = nullptr;
  int icon_size_ = 0;
  absl::variant<ColorId, SkColor> color_ = gfx::kPlaceholderColor;
  raw_ptr<const gfx::VectorIcon> badge_icon_ = nullptr;
};

// ImageModel encapsulates one of several image representations. See FromXXXX
// static-factory functions for supported formats.
class COMPONENT_EXPORT(UI_BASE) ImageModel {
 public:
  using ImageGenerator =
      base::RepeatingCallback<gfx::ImageSkia(const ui::ColorProvider*)>;

  ImageModel();
  ImageModel(const ImageModel&);
  ImageModel& operator=(const ImageModel&);
  ImageModel(ImageModel&&);
  ImageModel& operator=(ImageModel&&);
  ~ImageModel();

  // TODO(pkasting): Remove the default `color_id` or replace with kColorIcon.
  static ImageModel FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                   ColorId color_id = kColorMenuIcon,
                                   int icon_size = 0,
                                   const gfx::VectorIcon* badge_icon = nullptr);
  static ImageModel FromVectorIcon(const gfx::VectorIcon& vector_icon,
                                   SkColor color,
                                   int icon_size = 0,
                                   const gfx::VectorIcon* badge_icon = nullptr);
  static ImageModel FromImage(const gfx::Image& image);
  static ImageModel FromImageSkia(const gfx::ImageSkia& image_skia);
  // |FromResourceId| does not support color theming. To create an |ImageModel|
  // with color theming, use |ResourceBundle::GetThemedLottieImageNamed|.
  static ImageModel FromResourceId(int resource_id);
  // `size` must be the size of the image the `generator` returns.
  // NOTE: If this proves onerous, we could allow autodetection, at the cost of
  // requiring `generator` to be runnable with a null ColorProvider*.
  static ImageModel FromImageGenerator(ImageGenerator generator,
                                       gfx::Size size);

  bool IsEmpty() const;
  bool IsVectorIcon() const;
  bool IsImage() const;
  bool IsImageGenerator() const;
  gfx::Size Size() const;
  // Only valid if IsVectorIcon() or IsImage() return true, respectively.
  VectorIconModel GetVectorIcon() const;
  gfx::Image GetImage() const;
  ImageGenerator GetImageGenerator() const;

  // Checks if both models yield equal images.
  bool operator==(const ImageModel& other) const;
  bool operator!=(const ImageModel& other) const;

  // Rasterizes if necessary.
  gfx::ImageSkia Rasterize(const ui::ColorProvider* color_provider) const;

 private:
  struct ImageGeneratorAndSize {
    ImageGeneratorAndSize(ImageGenerator generator, gfx::Size size);
    ImageGeneratorAndSize(const ImageGeneratorAndSize&);
    ImageGeneratorAndSize& operator=(const ImageGeneratorAndSize&);
    ~ImageGeneratorAndSize();

    bool operator==(const ImageGeneratorAndSize& other) const;

    ImageGenerator generator;
    gfx::Size size;
  };

  explicit ImageModel(const VectorIconModel& vector_icon_model);
  explicit ImageModel(const gfx::Image& image);
  explicit ImageModel(const gfx::ImageSkia& image_skia);
  explicit ImageModel(ImageGeneratorAndSize image_generator);

  absl::variant<VectorIconModel, gfx::Image, ImageGeneratorAndSize> icon_;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_IMAGE_MODEL_H_
