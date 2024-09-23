// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider.h"

#include <map>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorProvider::ColorProvider() = default;

ColorProvider::~ColorProvider() = default;

ColorMixer& ColorProvider::AddMixer() {
  color_map_.clear();
  mixers_.emplace_after(
      first_postprocessing_mixer_,
      base::BindRepeating([](const ColorMixer* mixer) { return mixer; },
                          GetLastNonPostprocessingMixer()),
      base::BindRepeating(&ColorProvider::GetLastNonPostprocessingMixer,
                          base::Unretained(this)));

  return *std::next(first_postprocessing_mixer_, 1);
}

ColorMixer& ColorProvider::AddPostprocessingMixer() {
  color_map_.clear();
  if (first_postprocessing_mixer_ == mixers_.before_begin()) {
    // The first postprocessing mixer points to the last regular mixer.
    auto previous_mixer_getter = base::BindRepeating(
        &ColorProvider::GetLastNonPostprocessingMixer, base::Unretained(this));
    mixers_.emplace_front(previous_mixer_getter, previous_mixer_getter);
    first_postprocessing_mixer_ = mixers_.begin();
  } else {
    // Other postprocessing mixers point to the next postprocessing mixer.
    auto previous_mixer_getter =
        base::BindRepeating([](const ColorMixer* mixer) { return mixer; },
                            base::Unretained(&mixers_.front()));
    mixers_.emplace_front(previous_mixer_getter, previous_mixer_getter);
  }
  return mixers_.front();
}

SkColor ColorProvider::GetColor(ColorId id) const {
  auto i = color_map_.find(id);
  if (i == color_map_.end()) {
    if (mixers_.empty()) {
      DVLOG(2) << "ColorProvider::GetColor: No mixers defined!";
      return gfx::kPlaceholderColor;
    }
    DVLOG(2) << "ColorProvider::GetColor: Computing color for ColorId: "
             << ColorIdName(id);
    const SkColor color = mixers_.front().GetResultColor(id);
    if (color == gfx::kPlaceholderColor) {
      return gfx::kPlaceholderColor;
    }
    i = color_map_.insert({id, color}).first;
  }

  DVLOG(2) << "ColorProvider::GetColor: ColorId: " << ColorIdName(id)
           << " Value: " << SkColorName(i->second);
  return i->second;
}

void ColorProvider::SetColorForTesting(ColorId id, SkColor color) {
  color_map_[id] = color;
}

void ColorProvider::GenerateColorMapForTesting() {
  for (const auto& mixer : mixers_) {
    const auto mixer_color_ids = mixer.GetDefinedColorIds();
    for (const auto color_id : mixer_color_ids) {
      GetColor(color_id);
    }
  }
}

const ColorMixer* ColorProvider::GetLastNonPostprocessingMixer() const {
  const auto it = std::next(first_postprocessing_mixer_, 1);
  return (it == mixers_.cend()) ? nullptr : &(*it);
}

}  // namespace ui
