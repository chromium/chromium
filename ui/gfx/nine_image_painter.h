// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NINE_IMAGE_PAINTER_H_
#define UI_GFX_NINE_IMAGE_PAINTER_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

class Canvas;
class Insets;
class Rect;

class COMPONENT_EXPORT(GFX) NineImagePainter {
 public:
  explicit NineImagePainter(const std::vector<ImageSkia>& images);
  NineImagePainter(const ImageSkia& image, const Insets& insets);

  NineImagePainter(const NineImagePainter&) = delete;
  NineImagePainter& operator=(const NineImagePainter&) = delete;

  ~NineImagePainter();

  bool IsEmpty() const;
  Size GetMinimumSize() const;
  void Paint(Canvas* canvas, const Rect& bounds);
  void Paint(Canvas* canvas, const Rect& bounds, uint8_t alpha);

 private:
  friend class NineImagePainterTest;
  FRIEND_TEST_ALL_PREFIXES(NineImagePainterTest, GetSubsetRegions);

  // Gets the regions for the subimages into |regions|.
  static void GetSubsetRegions(const ImageSkia& image,
                               const Insets& insets,
                               std::vector<Rect>* regions);

  // Images are numbered as depicted below.
  //  ____________________
  // |__i0__|__i1__|__i2__|
  // |__i3__|__i4__|__i5__|
  // |__i6__|__i7__|__i8__|
  std::array<ImageSkia, 9> images_;
};

}  // namespace gfx

#endif  // UI_GFX_NINE_IMAGE_PAINTER_H_
