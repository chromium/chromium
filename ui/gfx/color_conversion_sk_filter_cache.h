// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
#define UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/hdr_metadata.h"

class SkColorFilter;
class SkRuntimeEffect;

namespace gfx {

class ColorTransform;

class COLOR_SPACE_EXPORT ColorConversionSkFilterCache {
 public:
  ColorConversionSkFilterCache();
  ColorConversionSkFilterCache(const ColorConversionSkFilterCache&) = delete;
  ColorConversionSkFilterCache& operator=(const ColorConversionSkFilterCache&) =
      delete;
  ~ColorConversionSkFilterCache();

  // Retrieve an SkColorFilter to transform `src` to `dst`. The bit depth of
  // `src` maybe specified in `src_bit_depth` (relevant only for YUV to RGB
  // conversion). Apply tone mapping of `src` is
  // HLG or PQ, using `src_hdr_metadata`, `dst_sdr_max_luminance_nits`, and
  // `dst_max_luminance_relative` as parameters.
  sk_sp<SkColorFilter> Get(const gfx::ColorSpace& src,
                           const gfx::ColorSpace& dst,
                           std::optional<uint32_t> src_bit_depth,
                           std::optional<gfx::HDRMetadata> src_hdr_metadata,
                           float dst_sdr_max_luminance_nits,
                           float dst_max_luminance_relative);

 public:
  struct Key {
    Key(const gfx::ColorSpace& src,
        uint32_t src_bit_depth,
        const gfx::ColorSpace& dst,
        float dst_sdr_max_luminance_nits);

    gfx::ColorSpace src;
    uint32_t src_bit_depth = 0;
    gfx::ColorSpace dst;
    float dst_sdr_max_luminance_nits = 0.f;

    bool operator==(const Key& other) const;
    bool operator!=(const Key& other) const;
    bool operator<(const Key& other) const;
  };
  struct Value {
    Value();
    Value(const Value&) = delete;
    Value(Value&&);
    Value& operator=(const Value&) = delete;
    Value& operator=(Value&&);
    ~Value();

    std::unique_ptr<ColorTransform> transform;
    sk_sp<SkRuntimeEffect> effect;
  };

  base::flat_map<Key, Value> cache_;
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
