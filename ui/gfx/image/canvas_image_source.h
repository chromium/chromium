// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_CANVAS_IMAGE_SOURCE_H_
#define UI_GFX_IMAGE_CANVAS_IMAGE_SOURCE_H_

#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"

namespace gfx {
class Canvas;
class ImageSkiaRep;
class Insets;

// CanvasImageSource is useful if you need to generate an image for a scale
// factor using Canvas. It creates a new Canvas with target scale factor and
// generates ImageSkiaRep when drawing is completed.
class GFX_EXPORT CanvasImageSource : public ImageSkiaSource {
 public:
  // Factory function to create an ImageSkia from a CanvasImageSource. Example:
  //   ImageSkia my_image =
  //       CanvasImageSource::MakeImageSkia<MySource>(param1, param2);
  template <typename T, typename... Args>
  static ImageSkia MakeImageSkia(Args&&... args) {
    auto source = std::make_unique<T>(std::forward<Args>(args)...);
    Size size = source->size();
    return ImageSkia(std::move(source), size);
  }

  // Creates a Image containing |image| with transparent padding around the
  // edges as specified by |insets|.
  static ImageSkia CreatePadded(const ImageSkia& image, const Insets& insets);

  explicit CanvasImageSource(const Size& size);
  ~CanvasImageSource() override {}

  // Called when a new image needs to be drawn for a scale factor.
  virtual void Draw(Canvas* canvas) = 0;

  // Returns the size of images in DIP that this source will generate.
  const Size& size() const { return size_; }

  // Overridden from ImageSkiaSource.
  ImageSkiaRep GetImageForScale(float scale) override;

 protected:
  const Size size_;
  DISALLOW_COPY_AND_ASSIGN(CanvasImageSource);
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_CANVAS_IMAGE_SOURCE_H_
