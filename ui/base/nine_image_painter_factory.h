// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NINE_IMAGE_PAINTER_FACTORY_H_
#define UI_BASE_NINE_IMAGE_PAINTER_FACTORY_H_

#include <memory>

#include "base/component_export.h"

// A macro to define arrays of IDR constants used with CreateImageGridPainter.
#define IMAGE_GRID(x) { x ## _TOP_LEFT,    x ## _TOP,    x ## _TOP_RIGHT, \
                        x ## _LEFT,        x ## _CENTER, x ## _RIGHT, \
                        x ## _BOTTOM_LEFT, x ## _BOTTOM, x ## _BOTTOM_RIGHT, }

// Defines an empty image for use in macros for creating an image grid for a
// ring of eight images.
#define EMPTY_IMAGE 0

// A macro to define arrays of IDR constants used with CreateImageGridPainter
// where only a ring of eight images is provided and center image is empty.
#define IMAGE_GRID_NO_CENTER(x) { x ## _TOP_LEFT, x ## _TOP, x ## _TOP_RIGHT, \
      x ## _LEFT, EMPTY_IMAGE, x ## _RIGHT, \
      x ## _BOTTOM_LEFT, x ## _BOTTOM, x ## _BOTTOM_RIGHT, }

// A macro to define arrays of IDR constants used with CreateImageGridPainter
// where it can only be streched horizontally.
#define IMAGE_GRID_HORIZONTAL(x) { x ## _LEFT, x ## _CENTER, x ## _RIGHT, \
      EMPTY_IMAGE, EMPTY_IMAGE, EMPTY_IMAGE, \
      EMPTY_IMAGE, EMPTY_IMAGE, EMPTY_IMAGE}

// A macro to define arrays of IDR constants used with CreateImageGridPainter
// where it can only be streched vertically.
#define IMAGE_GRID_VERTICAL(x) { x ## _TOP, EMPTY_IMAGE, EMPTY_IMAGE, \
        x ## _CENTER, EMPTY_IMAGE, EMPTY_IMAGE, \
        x ## _BOTTOM, EMPTY_IMAGE, EMPTY_IMAGE}

namespace gfx {
class NineImagePainter;
}

namespace ui {

// Creates a NineImagePainter from an array of image ids. It's expected the
// array came from the IMAGE_GRID macro.
COMPONENT_EXPORT(UI_BASE)
std::unique_ptr<gfx::NineImagePainter> CreateNineImagePainter(
    const int image_ids[]);

}  // namespace ui

#endif  // UI_BASE_NINE_IMAGE_PAINTER_FACTORY_H_
