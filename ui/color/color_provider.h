// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_H_
#define UI_COLOR_COLOR_PROVIDER_H_

#include <forward_list>
#include <map>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_variant.h"

namespace ui {

// A ColorProvider holds the complete pipeline of ColorMixers that compute
// result colors for UI elements.  ColorProvider is meant to be a long-lived
// object whose internal list of mixers does not change after initial
// construction.  Separate ColorProviders should be instantiated for e.g.
// windows with different themes.
// TODO(pkasting): Figure out ownership model and lifetime.
class COMPONENT_EXPORT(COLOR) ColorProvider {
 public:
  ColorProvider();
  // There should be no reason to copy or move a ColorProvider.
  ColorProvider(const ColorProvider&) = delete;
  ColorProvider& operator=(const ColorProvider&) = delete;
  ~ColorProvider();

  // Adds a mixer to the end of the current color pipeline.  Returns a reference
  // to the added mixer so callers can subsequently add sets and/or recipes.
  ColorMixer& AddMixer();

  // Returns the result color for |id| by applying the effects of each mixer in
  // order.  Returns gfx::kPlaceholderColor if no mixer knows how to construct
  // |id|.
  // TODO(pkasting): Current |variant| has no effect; figure out how to support
  // it.
  SkColor GetColor(ColorId id, ColorVariant variant = ColorVariant()) const;

 private:
  // The entire color pipeline, in reverse order (that is, the "last" mixer is
  // at the front).
  std::forward_list<ColorMixer> mixers_;

  // Caches the results of calls to GetColor().  This is invalidated by
  // AddMixer().  Uses a std::map rather than a base::flat_map since it has
  // frequent inserts and could grow very large.
  mutable std::map<ColorId, SkColor> cache_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_H_
