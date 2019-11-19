// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_SET_H_
#define UI_COLOR_COLOR_SET_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace ui {

// A ColorSet is a collection of identifiers mapped to actual SkColors, tagged
// with an overarching set ID.  This is intended to be used as input to a
// ColorMixer, where it can be referenced by ColorRecipes.  ColorSets are used
// to capture colors whose source is some external set of values.  Examples of
// plausible ColorSets are "the system native colors on this platform", "the
// core Chrome colors", "custom theme colors", and "native high-contrast
// colors".  ColorSets should consist of conceptually-related, orthogonal
// colors, and the ColorIds in them should be chosen to map well to the origin
// concepts, not to how those colors will be used in browser UI; use recipes to
// map input values from ColorSets to IDs that are referenced by UI elements, or
// to construct "pseudo-colors", transition values, and other colors that don't
// actually originate with an external source.
struct COMPONENT_EXPORT(COLOR) ColorSet {
  using ColorMap = base::flat_map<ColorId, SkColor>;

  ColorSet(ColorSetId id, ColorMap&& colors);
  ColorSet(ColorSet&&) noexcept;
  ColorSet& operator=(ColorSet&&) noexcept;
  ~ColorSet();

  ColorSetId id;
  ColorMap colors;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_SET_H_
