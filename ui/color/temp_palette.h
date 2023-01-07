// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_TEMP_PALETTE_H_
#define UI_COLOR_TEMP_PALETTE_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

enum class Luma {
  k0 = 0,
  k10 = 10,
  k20 = 20,
  k30 = 30,
  k40 = 40,
  k50 = 50,
  k60 = 60,
  k70 = 70,
  k80 = 80,
  k90 = 90,
  k95 = 95,
  k99 = 99,
  k100 = 100
};

// The tones generated from `seed` in the luma range.
struct COMPONENT_EXPORT(COLOR) ToneMap {
  ToneMap();
  ToneMap(const ToneMap&);
  ~ToneMap();

  SkColor seed;
  base::flat_map<Luma, SkColor> primary;
  base::flat_map<Luma, SkColor> secondary;
  base::flat_map<Luma, SkColor> tertiary;

  // Neutral tones
  base::flat_map<Luma, SkColor> neutral1;
  base::flat_map<Luma, SkColor> neutral2;

  // Error tones
  base::flat_map<Luma, SkColor> error;
};

// Returns the tones that closest match `seed_color`.
const ToneMap COMPONENT_EXPORT(COLOR) GetTempPalette(SkColor seed_color);

}  // namespace ui

#endif  // UI_COLOR_TEMP_PALETTE_H_
