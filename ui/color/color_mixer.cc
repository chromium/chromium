// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixer.h"

#include "base/logging.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorMixer::ColorMixer(const ColorMixer* previous_mixer,
                       MixerGetter input_mixer_getter)
    : previous_mixer_(previous_mixer),
      input_mixer_getter_(std::move(input_mixer_getter)) {}

ColorMixer::ColorMixer(ColorMixer&&) noexcept = default;

ColorMixer& ColorMixer::operator=(ColorMixer&&) noexcept = default;

ColorMixer::~ColorMixer() = default;

ColorRecipe& ColorMixer::operator[](ColorId id) {
  DCHECK_COLOR_ID_VALID(id);
  return recipes_[id];
}

void ColorMixer::AddSet(ColorSet&& set) {
  DCHECK(FindSetWithId(set.id) == sets_.cend());
  DVLOG(2) << "ColorSet " << ColorSetIdName(set.id) << " added.";
  sets_.push_front(std::move(set));
}

SkColor ColorMixer::GetInputColor(ColorId id) const {
  DCHECK_COLOR_ID_VALID(id);
  for (const auto& set : sets_) {
    const auto i = set.colors.find(id);
    if (i != set.colors.end()) {
      DVLOG(2) << "GetInputColor: ColorId " << ColorIdName(id)
               << " found within ColorSet " << ColorSetIdName(set.id)
               << " Result Color: " << SkColorName(i->second) << ".";
      return i->second;
    }
  }
  // Don't log transitions to previous mixers unless the logging level is a
  // little higher.
  DVLOG_IF(3, previous_mixer_)
      << "GetInputColor: ColorId " << ColorIdName(id) << " not found. "
      << "Checking previous mixer.";
  // If there's no previous mixer, always log color id misses.
  DVLOG_IF(2, !previous_mixer_)
      << "GetInputColor: ColorId " << ColorIdName(id) << " not found. "
      << "Returning gfx::kPlaceholderColor.";
  return previous_mixer_ ? previous_mixer_->GetResultColor(id)
                         : gfx::kPlaceholderColor;
}

SkColor ColorMixer::GetOriginalColorFromSet(ColorId id,
                                            ColorSetId set_id) const {
  DCHECK_COLOR_ID_VALID(id);
  DCHECK_COLOR_SET_ID_VALID(set_id);
  const auto i = FindSetWithId(set_id);
  if (i != sets_.end()) {
    const auto j = i->colors.find(id);
    if (j != i->colors.end()) {
      DVLOG(2) << "GetOriginalColorFromSet: ColorId " << ColorIdName(id)
               << " found within ColorSet " << ColorSetIdName(i->id)
               << " Result Color: " << SkColorName(j->second) << ".";
      return j->second;
    }
  }
  // Don't log transitions to previous mixers unless the logging level is a
  // little higher.
  DVLOG_IF(3, previous_mixer_)
      << "GetOriginalColorFromSet: ColorId " << ColorIdName(id)
      << " not found. Checking previous mixer.";
  // If there's no previous mixer, always log color id misses.
  DVLOG_IF(2, !previous_mixer_)
      << "GetOriginalColorFromSet: ColorId " << ColorIdName(id)
      << " not found. Returning gfx::kPlaceholderColor.";
  return previous_mixer_ ? previous_mixer_->GetOriginalColorFromSet(id, set_id)
                         : gfx::kPlaceholderColor;
}

SkColor ColorMixer::GetResultColor(ColorId id) const {
  DCHECK_COLOR_ID_VALID(id);
  const SkColor color = GetInputColor(id);
  const auto i = recipes_.find(id);
  const ColorMixer* const mixer =
      input_mixer_getter_ ? input_mixer_getter_.Run() : nullptr;
  return (i == recipes_.end())
             ? color
             : i->second.GenerateResult(color, *(mixer ? mixer : this));
}

ColorMixer::ColorSets::const_iterator ColorMixer::FindSetWithId(
    ColorSetId id) const {
  return std::find_if(sets_.cbegin(), sets_.cend(),
                      [id](const auto& set) { return set.id == id; });
}

}  // namespace ui
