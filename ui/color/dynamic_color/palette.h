// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_DYNAMIC_COLOR_PALETTE_H_
#define UI_COLOR_DYNAMIC_COLOR_PALETTE_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

// For a given color, provides the color values for the tonal range between 0
// and 100 in the HCT color space.
class COMPONENT_EXPORT(DYNAMIC_COLOR) TonalPalette {
 public:
  explicit TonalPalette(SkColor seed_color);
  TonalPalette(double hue, double chroma);

  TonalPalette(const TonalPalette& other);
  TonalPalette& operator=(const TonalPalette&);

  ~TonalPalette();

  // Returns the color in the palette for the corresponding `tone`.
  // `tone` must be between 0.f and 100.f or SKColor_TRANSPARENT will be
  // returned.
  SkColor get(float tone) const;

 private:
  double hue_;
  double chroma_;
};

// A collection of TonalPalettes representing the reference palette for a
// particular color configuration.
class COMPONENT_EXPORT(DYNAMIC_COLOR) Palette {
 public:
  virtual ~Palette() = default;

  virtual const TonalPalette& primary() const = 0;
  virtual const TonalPalette& secondary() const = 0;
  virtual const TonalPalette& tertiary() const = 0;
  virtual const TonalPalette& neutral() const = 0;
  virtual const TonalPalette& neutral_variant() const = 0;
  virtual const TonalPalette& error() const = 0;
};

}  // namespace ui

#endif  // UI_COLOR_DYNAMIC_COLOR_PALETTE_H_
