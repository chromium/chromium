// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_MIXER_H_
#define UI_COLOR_COLOR_MIXER_H_

#include <forward_list>
#include <map>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_set.h"

namespace ui {

class ColorRecipe;

// ColorMixer represents a single conceptual mapping of a set of inputs, via a
// collection of transforms, to a set of outputs.  Examples of plausible
// ColorMixers are "the UI element colors, as constructed from core color
// primitive values", "the way a set of high contrast colors overwrites default
// values", "the final output colors for all parts of a single UI area", or
// "a layer that enforces contrast minima on a variety of inputs".  ColorMixers
// are chained together into a pipeline by a ColorProvider, and thus may rely
// completely, partly, or not at all on the inputs and outputs of previous
// mixers in the pipeline.
class COMPONENT_EXPORT(COLOR) ColorMixer {
 public:
  // Having each ColorMixer know about the |previous_mixer| in the pipeline
  // allows mixers to implement the pipeline directly and simplifies the API,
  // compared to having each mixer report results (via e.g. Optional<SkColor>)
  // to the ColorProvider, which would need to query different mixers in order.
  explicit ColorMixer(const ColorMixer* previous_mixer = nullptr);
  // ColorMixer is movable since it holds both sets and recipes, each of which
  // might be expensive to copy.
  ColorMixer(ColorMixer&&) noexcept;
  ColorMixer& operator=(ColorMixer&&) noexcept;
  ~ColorMixer();

  // Adds a recipe for |id| if it does not exist.
  ColorRecipe& operator[](ColorId id);

  // Adds |set| to |sets_|.  |set| must not have the same ID as any previously
  // added sets, though it may contain colors with the same IDs as colors in
  // those sets; in such cases, the last-added set takes priority.
  void AddSet(ColorSet&& set);

  // Returns the input color for |id|.  First searches all |sets_| in reverse
  // order; if not found, asks the previous mixer for the result color.  If
  // there is no previous mixer, returns gfx::kPlaceholderColor.
  SkColor GetInputColor(ColorId id) const;

  // Returns the color for |id| from |set_id|.  If this mixer does not have that
  // set, the request will be forwarded to the previous mixer.  If there is no
  // previous mixer, returns gfx::kPlaceholderColor.
  SkColor GetOriginalColorFromSet(ColorId id, ColorSetId set_id) const;

  // Returns the result color for |id|, that is, the result of applying any
  // applicable recipe from |recipes_| to the relevant input color.
  SkColor GetResultColor(ColorId id) const;

 private:
  using ColorSets = std::forward_list<ColorSet>;

  // Returns an iterator to the set in |sets_| with ID |id|, or sets_.cend().
  ColorSets::const_iterator FindSetWithId(ColorSetId id) const;

  const ColorMixer* previous_mixer_;
  ColorSets sets_;

  // This uses std::map instead of base::flat_map since the recipes are inserted
  // one at a time instead of all at once, and there may be a lot of them.
  // TODO(pkasting): Consider unifying how sets and recipes are specified:
  // either both at construction (at which point this can use a flat_map) or
  // both built piecemeal (which would mean ColorSets should probably become a
  // std::map as well).
  std::map<ColorId, ColorRecipe> recipes_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_MIXER_H_
