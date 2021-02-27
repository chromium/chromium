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

ColorMixer& ColorProvider::AddMixer() {
  // Adding a mixer could change any of the result colors.
  cache_.clear();

  // Supply each new mixer with the previous mixer in the pipeline; this way
  // GetColor() need not query each mixer in order, but simply ask the last
  // mixer for its result, and trust mixers to query each other back up the
  // chain as needed.
  mixers_.emplace_front(mixers_.empty() ? nullptr : &mixers_.front());
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

ColorProvider::ColorProvider() = default;

ColorProvider::~ColorProvider() = default;

}  // namespace ui
