// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_H_
#define UI_COLOR_COLOR_PROVIDER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"

namespace ui {

// A ColorProvider holds the complete pipeline of ColorMixers that compute
// result colors for UI elements.  ColorProvider is meant to be a long-lived
// object whose internal list of mixers does not change after initial
// construction.  Separate ColorProviders should be instantiated for e.g.
// windows with different themes.
// TODO(pkasting): Figure out ownership model and lifetime.
class COMPONENT_EXPORT(COLOR) ColorProvider {
 public:
  using ColorMap = std::map<ColorId, SkColor>;

  ColorProvider();
  ColorProvider(const ColorProvider&) = delete;
  ColorProvider& operator=(const ColorProvider&) = delete;
  ColorProvider(ColorProvider&&);
  ColorProvider& operator=(ColorProvider&&);
  ~ColorProvider();

  // Adds a mixer to the current color pipeline after all other
  // non-"postprocessing" mixers.  Returns a reference to the added mixer so
  // callers can subsequently add sets and/or recipes.
  ColorMixer& AddMixer();

  // Like AddMixer(), but adds at the very end of the color pipeline.
  // "Postprocessing" mixers are meant to run after all other mixers and are
  // skipped when calling GetUnprocessedColor().
  ColorMixer& AddPostprocessingMixer();

  // Returns the result color for |id| by applying the effects of each mixer in
  // order.  Returns gfx::kPlaceholderColor if no mixer knows how to construct
  // |id|.
  SkColor GetColor(ColorId id) const;

  void SetColorForTesting(ColorId id, SkColor color);
  void GenerateColorMapForTesting();
  const ColorMap& color_map_for_testing();

 private:
  // ColorProviderInternal provides the actual implementation of ColorProvider.
  // It's non-movable and non-copyable so that ColorMixer's callbacks can safely
  // bind to it.
  class ColorProviderInternal;

  std::unique_ptr<ColorProviderInternal> internal_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_H_
