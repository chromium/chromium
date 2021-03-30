// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider.h"

#include <utility>

#include "base/logging.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/color_palette.h"

namespace ui {

ColorProvider::ColorProvider() = default;

ColorProvider::~ColorProvider() = default;

ColorMixer& ColorProvider::AddMixer() {
  // Adding a mixer could change any of the result colors.
  cache_.clear();

  mixers_.emplace_after(first_postprocessing_mixer_,
                        GetLastNonPostprocessingMixer(),
                        base::BindRepeating(
                            [](const ColorProvider* provider) {
                              return provider->GetLastNonPostprocessingMixer();
                            },
                            base::Unretained(this)));
  return *std::next(first_postprocessing_mixer_, 1);
}

ColorMixer& ColorProvider::AddPostprocessingMixer() {
  // Adding a mixer could change any of the result colors.
  cache_.clear();

  if (first_postprocessing_mixer_ == mixers_.before_begin()) {
    mixers_.emplace_front(
        mixers_.empty() ? nullptr : &mixers_.front(),
        base::BindRepeating(
            [](const ColorProvider* provider) {
              return provider->GetLastNonPostprocessingMixer();
            },
            base::Unretained(this)));
    first_postprocessing_mixer_ = mixers_.begin();
  } else {
    mixers_.emplace_front(
        &mixers_.front(),
        base::BindRepeating([](const ColorMixer* mixer) { return mixer; },
                            base::Unretained(&mixers_.front())));
  }
  return mixers_.front();
}

SkColor ColorProvider::GetColor(ColorId id) const {
  DCHECK_COLOR_ID_VALID(id);

  if (mixers_.empty()) {
    DVLOG(2) << "ColorProvider::GetColor: No mixers defined!";
    return gfx::kPlaceholderColor;
  }

  // Only compute the result color when it's not already in the cache.
  auto i = cache_.find(id);
  if (i == cache_.end()) {
    DVLOG(2) << "ColorProvider::GetColor: Computing color for ColorId: "
             << ColorIdName(id);
    i = cache_.insert({id, mixers_.front().GetResultColor(id)}).first;
  }

  DVLOG(2) << "ColorProvider::GetColor: ColorId: " << ColorIdName(id)
           << " Value: " << SkColorName(i->second);
  return i->second;
}

const ColorMixer* ColorProvider::GetLastNonPostprocessingMixer() const {
  const auto it = std::next(first_postprocessing_mixer_, 1);
  return (it == mixers_.cend()) ? nullptr : &(*it);
}

}  // namespace ui
