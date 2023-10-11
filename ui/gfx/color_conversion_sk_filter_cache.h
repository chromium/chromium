// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_
#define UI_GFX_COLOR_CONVERSION_SK_FILTER_CACHE_H_

#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/hdr_metadata.h"

class GrDirectContext;
class SkImage;
class SkColorFilter;
class SkRuntimeEffect;
struct SkGainmapInfo;

namespace skgpu::graphite {
class Recorder;
}

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
  // conversion). The filter also applies the offset `src_resource_offset` and
  // then scales by `src_resource_multiplier`. Apply tone mapping of `src` is
  // HLG or PQ, using `src_hdr_metadata`, `dst_sdr_max_luminance_nits`, and
  // `dst_max_luminance_relative` as parameters.
  sk_sp<SkColorFilter> Get(const gfx::ColorSpace& src,
                           const gfx::ColorSpace& dst,
                           float resource_offset,
                           float resource_multiplier,
                           absl::optional<uint32_t> src_bit_depth,
                           absl::optional<gfx::HDRMetadata> src_hdr_metadata,
                           float dst_sdr_max_luminance_nits,
                           float dst_max_luminance_relative);

  // Return if ApplyToneCurve can be called on `image`.
  static bool UseToneCurve(sk_sp<SkImage> image);

  // Perform global tone mapping on `image`, using `dst_sdr_max_luminance_nits`,
  // `dst_max_luminance_relative`, and `src_hdr_metadata`. The resulting image
  // will be in Rec2020 linear space, and will not have mipmaps.
  sk_sp<SkImage> ApplyToneCurve(sk_sp<SkImage> image,
                                absl::optional<HDRMetadata> src_hdr_metadata,
                                float dst_sdr_max_luminance_nits,
                                float dst_max_luminance_relative,
                                GrDirectContext* gr_context,
                                skgpu::graphite::Recorder* graphite_recorder);

  // Apply the gainmap in `gainmap_image` to `base_image`, using the parameters
  // in `gainmap_info` and `dst_max_luminance_relative`, and return the
  // resulting image.
  // * If `context` is non-nullptr, then `base_image` and `gainmap_image` must
  //   be texture-backed and on `context`, and the result will be texture backed
  //   and on `context`.
  // * If `context` is nullptr, then the arguments should be bitmaps, and the
  //   result will be a bitmap.
  sk_sp<SkImage> ApplyGainmap(sk_sp<SkImage> base_image,
                              sk_sp<SkImage> gainmap_image,
                              const SkGainmapInfo& gainmap_info,
                              float dst_max_luminance_relative,
                              GrDirectContext* gr_context,
                              skgpu::graphite::Recorder* graphite_recorder);

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
