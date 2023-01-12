// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_MIXER_H_
#define UI_COLOR_COLOR_MIXER_H_

#include <map>
#include <set>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace ui {

class ColorProvider;
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
  using MixerGetter = base::RepeatingCallback<const ColorMixer*(void)>;

  // Having each ColorMixer know about the |previous mixer| in the pipeline
  // allows mixers to implement the pipeline directly and simplifies the API,
  // compared to having each mixer report results (via e.g. Optional<SkColor>)
  // to the ColorProvider, which would need to query different mixers in order.
  // |input_mixer_getter| can be .Run() to obtain the appropriate mixer to
  // query for transform inputs.
  explicit ColorMixer(MixerGetter previous_mixer_getter = base::NullCallback(),
                      MixerGetter input_mixer_getter = base::NullCallback());
  // ColorMixer is movable since it holds both sets and recipes, each of which
  // might be expensive to copy.
  ColorMixer(ColorMixer&&) noexcept;
  ColorMixer& operator=(ColorMixer&&) noexcept;
  ~ColorMixer();

  // Adds a recipe for |id| if it does not exist.
  ColorRecipe& operator[](ColorId id);

  // Returns the input color for |id|.  First searches all |sets_| in reverse
  // order; if not found, asks the previous mixer for the result color.  If
  // there is no previous mixer, returns gfx::kPlaceholderColor.
  SkColor GetInputColor(ColorId id) const;

  // Returns the result color for |id|, that is, the result of applying any
  // applicable recipe from |recipes_| to the relevant input color.
  SkColor GetResultColor(ColorId id) const;

  // Returns the ColorIds defined for this mixer.
  std::set<ColorId> GetDefinedColorIds() const;

 private:
  MixerGetter previous_mixer_getter_;
  MixerGetter input_mixer_getter_;

  // This uses std::map instead of base::flat_map since the recipes are inserted
  // one at a time instead of all at once, and there may be a lot of them.
  std::map<ColorId, ColorRecipe> recipes_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_MIXER_H_
