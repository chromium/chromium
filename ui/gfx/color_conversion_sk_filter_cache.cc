// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversion_sk_filter_cache.h"

#include <string>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

namespace {

// Additional YUV information to skia renderer to draw 9- and 10- bits color.
struct YUVInput {
  float offset = 0.f;
  float multiplier = 0.f;
};

}  // namespace

ColorConversionSkFilterCache::ColorConversionSkFilterCache() = default;
ColorConversionSkFilterCache::~ColorConversionSkFilterCache() = default;

bool ColorConversionSkFilterCache::Key::Key::operator==(
    const Key& other) const {
  return src == other.src && dst == other.dst &&
         sdr_max_luminance_nits == other.sdr_max_luminance_nits;
}

bool ColorConversionSkFilterCache::Key::operator!=(const Key& other) const {
  return !(*this == other);
}

bool ColorConversionSkFilterCache::Key::operator<(const Key& other) const {
  return std::tie(src, dst, sdr_max_luminance_nits) <
         std::tie(other.src, other.dst, other.sdr_max_luminance_nits);
}

ColorConversionSkFilterCache::Key::Key(const gfx::ColorSpace& src,
                                       const gfx::ColorSpace& dst,
                                       float sdr_max_luminance_nits)
    : src(src), dst(dst), sdr_max_luminance_nits(sdr_max_luminance_nits) {}

sk_sp<SkColorFilter> ColorConversionSkFilterCache::Get(
    const gfx::ColorSpace& src,
    const gfx::ColorSpace& dst,
    float resource_offset,
    float resource_multiplier,
    float sdr_max_luminance_nits,
    float dst_max_luminance_relative) {
  const Key key(src, dst, sdr_max_luminance_nits);
  sk_sp<SkRuntimeEffect>& effect = cache_[key];

  if (!effect) {
    gfx::ColorTransform::Options options;
    options.tone_map_pq_and_hlg_to_sdr = !key.dst.IsHDR();
    options.sdr_max_luminance_nits = key.sdr_max_luminance_nits;
    // TODO(https://crbug.com/1286076): Ensure that, when tone mapping using
    // `dst_max_luminance_relative` is implemented, the gfx::ColorTransform's
    // SkShaderSource not depend on that parameter (rather, that it be left
    // as a uniform in the shader). If that is not the case, then it will need
    // to be part of the key.
    options.dst_max_luminance_relative = dst_max_luminance_relative;
    std::unique_ptr<gfx::ColorTransform> transform =
        gfx::ColorTransform::NewColorTransform(src, dst, options);

    const char* hdr = R"(
uniform half offset;
uniform half multiplier;

half4 main(half4 color) {
  // un-premultiply alpha
  if (color.a > 0)
    color.rgb /= color.a;

  color.rgb -= offset;
  color.rgb *= multiplier;
)";
    const char* ftr = R"(
  // premultiply alpha
  color.rgb *= color.a;
  return color;
}
)";

    std::string shader = hdr + transform->GetSkShaderSource() + ftr;

    auto result = SkRuntimeEffect::MakeForColorFilter(
        SkString(shader.c_str(), shader.size()),
        /*options=*/{});
    DCHECK(result.effect) << std::endl
                          << result.errorText.c_str() << "\n\nShader Source:\n"
                          << shader;
    effect = result.effect;
  }

  YUVInput input;
  input.offset = resource_offset;
  input.multiplier = resource_multiplier;
  sk_sp<SkData> data = SkData::MakeWithCopy(&input, sizeof(input));

  return effect->makeColorFilter(std::move(data));
}

}  // namespace gfx
