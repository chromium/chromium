// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversion_sk_filter_cache.h"

#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

ColorConversionSkFilterCache::ColorConversionSkFilterCache() = default;
ColorConversionSkFilterCache::~ColorConversionSkFilterCache() = default;

bool ColorConversionSkFilterCache::Key::Key::operator==(
    const Key& other) const {
  return src == other.src && src_bit_depth == other.src_bit_depth &&
         dst == other.dst &&
         dst_sdr_max_luminance_nits == other.dst_sdr_max_luminance_nits;
}

bool ColorConversionSkFilterCache::Key::operator!=(const Key& other) const {
  return !(*this == other);
}

bool ColorConversionSkFilterCache::Key::operator<(const Key& other) const {
  return std::tie(src, src_bit_depth, dst, dst_sdr_max_luminance_nits) <
         std::tie(other.src, other.src_bit_depth, other.dst,
                  other.dst_sdr_max_luminance_nits);
}

ColorConversionSkFilterCache::Key::Key(const gfx::ColorSpace& src,
                                       uint32_t src_bit_depth,
                                       const gfx::ColorSpace& dst,
                                       float dst_sdr_max_luminance_nits)
    : src(src),
      src_bit_depth(src_bit_depth),
      dst(dst),
      dst_sdr_max_luminance_nits(dst_sdr_max_luminance_nits) {}

ColorConversionSkFilterCache::Value::Value() = default;

ColorConversionSkFilterCache::Value::Value(Value&& other)
    : transform(std::move(other.transform)), effect(std::move(other.effect)) {}

ColorConversionSkFilterCache::Value&
ColorConversionSkFilterCache::Value::operator=(Value&& other) {
  transform = std::move(other.transform);
  effect = std::move(other.effect);
  return *this;
}

ColorConversionSkFilterCache::Value::~Value() = default;

sk_sp<SkColorFilter> ColorConversionSkFilterCache::Get(
    const gfx::ColorSpace& src,
    const gfx::ColorSpace& dst,
    std::optional<uint32_t> src_bit_depth,
    std::optional<gfx::HDRMetadata> src_hdr_metadata,
    float dst_sdr_max_luminance_nits,
    float dst_max_luminance_relative) {
  // Set unused parameters to bogus values, so that they do not result in
  // different keys for the same conversion.
  if (!src.IsToneMappedByDefault()) {
    // If the source is not going to be tone mapped, then `src_hdr_metadata`
    // and `dst_max_luminance_relative` will not be used, so set them nonsense
    // values.
    src_hdr_metadata = std::nullopt;
    dst_max_luminance_relative = 0;

    // If neither source nor destination will use `dst_sdr_max_luminance_nits`,
    // then set it to a nonsense value.
    if (!dst.IsAffectedBySDRWhiteLevel() && !src.IsAffectedBySDRWhiteLevel()) {
      dst_sdr_max_luminance_nits = 0;
    }
  }

  const Key key(src, src_bit_depth.value_or(0), dst,
                dst_sdr_max_luminance_nits);
  Value& value = cache_[key];

  if (!value.effect) {
    gfx::ColorTransform::Options options;
    options.tone_map_pq_and_hlg_to_dst = true;
    if (src_bit_depth) {
      options.src_bit_depth = src_bit_depth.value();
    }
    value.transform = gfx::ColorTransform::NewColorTransform(src, dst, options);
    value.effect = value.transform->GetSkRuntimeEffect();
  }

  gfx::ColorTransform::RuntimeOptions options;
  options.offset = 0.0f;
  options.multiplier = 1.0f;
  options.src_hdr_metadata = src_hdr_metadata;
  options.dst_sdr_max_luminance_nits = dst_sdr_max_luminance_nits;
  options.dst_max_luminance_relative = dst_max_luminance_relative;
  return value.effect->makeColorFilter(
      value.transform->GetSkShaderUniforms(options));
}

}  // namespace gfx
