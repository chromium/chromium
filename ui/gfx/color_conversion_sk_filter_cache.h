// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
#define UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_

#include "base/containers/flat_map.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class GFX_EXPORT ColorConversionSkFilterCache {
 public:
  ColorConversionSkFilterCache();
  ColorConversionSkFilterCache(const ColorConversionSkFilterCache&) = delete;
  ColorConversionSkFilterCache& operator=(const ColorConversionSkFilterCache&) =
      delete;
  ~ColorConversionSkFilterCache();

  sk_sp<SkColorFilter> Get(const gfx::ColorSpace& src,
                           const gfx::ColorSpace& dst,
                           float resource_offset,
                           float resource_multiplier,
                           float sdr_max_luminance_nits,
                           float dst_max_luminance_relative);

 public:
  struct Key {
    Key(const gfx::ColorSpace& src,
        const gfx::ColorSpace& dst,
        float sdr_max_luminance_nits);

    gfx::ColorSpace src;
    gfx::ColorSpace dst;
    float sdr_max_luminance_nits = 0.f;

    bool operator==(const Key& other) const;
    bool operator!=(const Key& other) const;
    bool operator<(const Key& other) const;
  };
  static Key KeyForParams(const gfx::ColorSpace& src,
                          const gfx::ColorSpace& dst,
                          float resource_offset,
                          float resource_multiplier,
                          float sdr_max_luminance_nits,
                          float dst_max_luminance_relative);

  base::flat_map<Key, sk_sp<SkRuntimeEffect>> cache_;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
