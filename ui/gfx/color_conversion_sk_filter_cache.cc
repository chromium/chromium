// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversion_sk_filter_cache.h"

#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

namespace {

const base::Feature kImageToneMapping{"ImageToneMapping",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

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
         sdr_max_luminance_nits == other.sdr_max_luminance_nits &&
         dst_max_luminance_relative == other.dst_max_luminance_relative;
}

bool ColorConversionSkFilterCache::Key::operator!=(const Key& other) const {
  return !(*this == other);
}

bool ColorConversionSkFilterCache::Key::operator<(const Key& other) const {
  return std::tie(src, dst, sdr_max_luminance_nits,
                  dst_max_luminance_relative) <
         std::tie(other.src, other.dst, other.sdr_max_luminance_nits,
                  other.dst_max_luminance_relative);
}

ColorConversionSkFilterCache::Key::Key(const gfx::ColorSpace& src,
                                       const gfx::ColorSpace& dst,
                                       float sdr_max_luminance_nits,
                                       float dst_max_luminance_relative)
    : src(src),
      dst(dst),
      sdr_max_luminance_nits(sdr_max_luminance_nits),
      dst_max_luminance_relative(dst_max_luminance_relative) {}

sk_sp<SkColorFilter> ColorConversionSkFilterCache::Get(
    const gfx::ColorSpace& src,
    const gfx::ColorSpace& dst,
    float resource_offset,
    float resource_multiplier,
    float sdr_max_luminance_nits,
    float dst_max_luminance_relative) {
  // Set unused parameters to bogus values, so that they do not result in
  // different keys for the same conversion.
  if (!src.IsPQOrHLG()) {
    // If the source is not HLG or PQ, then `dst_max_luminance_relative` will
    // not be used, so set it to a nonsense value.
    dst_max_luminance_relative = 0;

    // If neither source nor destination are HLG or PQ, then
    // `sdr_max_luminance_nits` will not be used, so set it to a nonsense value.
    if (!dst.IsPQOrHLG()) {
      sdr_max_luminance_nits = 0;
    }
  }

  const Key key(src, dst, sdr_max_luminance_nits, dst_max_luminance_relative);
  sk_sp<SkRuntimeEffect>& effect = cache_[key];

  if (!effect) {
    gfx::ColorTransform::Options options;
    options.tone_map_pq_and_hlg_to_sdr = dst_max_luminance_relative == 1.f;
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

sk_sp<SkImage> ColorConversionSkFilterCache::ConvertImage(
    sk_sp<SkImage> image,
    sk_sp<SkColorSpace> target_color_space,
    float sdr_max_luminance_nits,
    float dst_max_luminance_relative,
    GrDirectContext* context) {
  static bool image_tone_mapping_enabled =
      base::FeatureList::IsEnabled(kImageToneMapping);

  sk_sp<SkColorSpace> image_sk_color_space = image->refColorSpace();
  if (!image_sk_color_space || !image_tone_mapping_enabled)
    return image->makeColorSpace(target_color_space, context);

  gfx::ColorSpace image_color_space(*image_sk_color_space);
  switch (image_color_space.GetTransferID()) {
    case ColorSpace::TransferID::PQ:
    case ColorSpace::TransferID::HLG:
      break;
    default:
      return image->makeColorSpace(target_color_space, context);
  }

  SkImageInfo image_info =
      SkImageInfo::Make(image->dimensions(),
                        SkColorInfo(kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                                    image_sk_color_space));
  sk_sp<SkSurface> surface;
  if (context) {
    // TODO(https://crbug.com/1286088): Consider adding mipmap support here.
    surface =
        SkSurface::MakeRenderTarget(context, SkBudgeted::kNo, image_info,
                                    /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
                                    /*surfaceProps=*/nullptr,
                                    /*shouldCreateWithMips=*/false);
    // It is not guaranteed that kRGBA_F16_SkColorType is renderable. If we fail
    // to create an SkSurface with that color type, fall back to
    // kN32_SkColorType.
    if (!surface) {
      DLOG(ERROR) << "Falling back to tone mapped 8-bit surface.";
      image_info = image_info.makeColorType(kN32_SkColorType);
      surface = SkSurface::MakeRenderTarget(
          context, SkBudgeted::kNo, image_info,
          /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
          /*surfaceProps=*/nullptr,
          /*shouldCreateWithMips=*/false);
    }
  } else {
    surface = SkSurface::MakeRaster(image_info, image_info.minRowBytes(),
                                    /*surfaceProps=*/nullptr);
  }
  if (!surface) {
    DLOG(ERROR) << "Failed to create SkSurface color conversion.";
    return nullptr;
  }

  sk_sp<SkColorFilter> filter =
      Get(image_color_space, gfx::ColorSpace(*target_color_space),
          /*resource_offset=*/0, /*resource_multiplier=*/1,
          sdr_max_luminance_nits, dst_max_luminance_relative);
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  paint.setColorFilter(filter);
  SkSamplingOptions sampling_options(SkFilterMode::kNearest);
  surface->getCanvas()->drawImage(image,
                                  /*x=*/0, /*y=*/0, sampling_options, &paint);
  return surface->makeImageSnapshot()->reinterpretColorSpace(
      target_color_space);
}

}  // namespace gfx
