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
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "third_party/skia/include/private/SkGainmapInfo.h"
#include "third_party/skia/include/private/SkGainmapShader.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

namespace {

// Allocate an SkSurface to be used to create the tonemapped result.
static sk_sp<SkSurface> MakeSurfaceForResult(
    SkImageInfo image_info,
    GrDirectContext* gr_context,
    skgpu::graphite::Recorder* graphite_recorder) {
  // TODO(ccameron) this code is only used in OOP-R, which implies a GPU
  // backend, so perhaps this code should be moved to cc/
#if defined(SK_GANESH)
  if (gr_context) {
    // TODO(https://crbug.com/1286088): Consider adding mipmap support here.
    sk_sp<SkSurface> surface =
        SkSurfaces::RenderTarget(gr_context, skgpu::Budgeted::kNo, image_info,
                                 /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
                                 /*surfaceProps=*/nullptr,
                                 /*shouldCreateWithMips=*/false);
    // It is not guaranteed that kRGBA_F16_SkColorType is renderable. If we fail
    // to create an SkSurface with that color type, fall back to
    // kN32_SkColorType.
    if (surface) {
      return surface;
    }
    DLOG(ERROR) << "Falling back to tone mapped 8-bit surface.";
    image_info = image_info.makeColorType(kN32_SkColorType);
    return SkSurfaces::RenderTarget(gr_context, skgpu::Budgeted::kNo,
                                    image_info,
                                    /*sampleCount=*/0, kTopLeft_GrSurfaceOrigin,
                                    /*surfaceProps=*/nullptr,
                                    /*shouldCreateWithMips=*/false);
  }
#endif
#if defined(SK_GRAPHITE)
  if (graphite_recorder) {
    // TODO(https://crbug.com/1286088): Consider adding mipmap support here.
    sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
        graphite_recorder, image_info, skgpu::Mipmapped::kNo,
        /*surfaceProps=*/nullptr);
    // It is not guaranteed that kRGBA_F16_SkColorType is renderable. If we fail
    // to create an SkSurface with that color type, fall back to
    // kN32_SkColorType.
    if (surface) {
      return surface;
    }
    DLOG(ERROR) << "Falling back to tone mapped 8-bit surface.";
    image_info = image_info.makeColorType(kN32_SkColorType);
    return SkSurfaces::RenderTarget(graphite_recorder, image_info,
                                    skgpu::Mipmapped::kNo,
                                    /*surfaceProps=*/nullptr);
  }
#endif
  return SkSurfaces::Raster(image_info, image_info.minRowBytes(),
                            /*surfaceProps=*/nullptr);
}

}  // namespace

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
    float resource_offset,
    float resource_multiplier,
    absl::optional<uint32_t> src_bit_depth,
    absl::optional<gfx::HDRMetadata> src_hdr_metadata,
    float dst_sdr_max_luminance_nits,
    float dst_max_luminance_relative) {
  // Set unused parameters to bogus values, so that they do not result in
  // different keys for the same conversion.
  if (!src.IsToneMappedByDefault()) {
    // If the source is not going to be tone mapped, then `src_hdr_metadata`
    // and `dst_max_luminance_relative` will not be used, so set them nonsense
    // values.
    src_hdr_metadata = absl::nullopt;
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
  options.offset = resource_offset;
  options.multiplier = resource_multiplier;
  options.src_hdr_metadata = src_hdr_metadata;
  options.dst_sdr_max_luminance_nits = dst_sdr_max_luminance_nits;
  options.dst_max_luminance_relative = dst_max_luminance_relative;
  return value.effect->makeColorFilter(
      value.transform->GetSkShaderUniforms(options));
}

sk_sp<SkImage> ColorConversionSkFilterCache::ApplyGainmap(
    sk_sp<SkImage> base_image,
    sk_sp<SkImage> gainmap_image,
    const SkGainmapInfo& gainmap_info,
    float dst_max_luminance_relative,
    GrDirectContext* gr_context,
    skgpu::graphite::Recorder* graphite_recorder) {
  DCHECK_EQ(base_image->isTextureBacked(), gainmap_image->isTextureBacked());
  CHECK_EQ((!!gr_context || !!graphite_recorder),
           base_image->isTextureBacked());

  // If `gainmap_image` will not be applied, then return `base_image` directly.
  switch (gainmap_info.fBaseImageType) {
    case SkGainmapInfo::BaseImageType::kSDR:
      if (dst_max_luminance_relative <= gainmap_info.fDisplayRatioSdr) {
        return base_image;
      }
      break;
    case SkGainmapInfo::BaseImageType::kHDR:
      if (dst_max_luminance_relative >= gainmap_info.fDisplayRatioHdr) {
        return base_image;
      }
      break;
  }

  // The output surface will be in a linearized version of the input
  // base_image's color space.
  sk_sp<SkColorSpace> surface_color_space = base_image->refColorSpace();
  if (surface_color_space) {
    surface_color_space = surface_color_space->makeLinearGamma();
  } else {
    surface_color_space = SkColorSpace::MakeSRGBLinear();
  }
  SkImageInfo surface_info =
      SkImageInfo::Make(base_image->dimensions(),
                        SkColorInfo(kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                                    surface_color_space));

  // Create the surface to render the gainmap shader to.
  sk_sp<SkSurface> surface =
      MakeSurfaceForResult(surface_info, gr_context, graphite_recorder);
  if (!surface) {
    LOG(ERROR) << "Failed to create SkSurface for applying gainmap.";
    return base_image;
  }

  // Use nearest-neighbor interpolation for the base image (it is the same size
  // as the surface, so no interpolation will be done anyway) and linear
  // interpolation for the gainmap (it is often 1/4 width and 1/4 height of the
  // base image).
  const SkSamplingOptions base_sampling_options(SkFilterMode::kNearest);
  const SkSamplingOptions gainmap_sampling_options(SkFilterMode::kLinear);

  // Render the gainmap shader to the surface.
  SkRect image_rect = SkRect::MakeSize(SkSize::Make(base_image->dimensions()));
  SkRect gainmap_rect =
      SkRect::MakeSize(SkSize::Make(gainmap_image->dimensions()));
  SkRect surface_rect =
      SkRect::MakeSize(SkSize::Make(surface_info.dimensions()));
  sk_sp<SkShader> shader = SkGainmapShader::Make(
      base_image, image_rect, base_sampling_options, gainmap_image,
      gainmap_rect, gainmap_sampling_options, gainmap_info, surface_rect,
      dst_max_luminance_relative, surface_color_space);
  DCHECK(shader);
  SkPaint paint;
  paint.setShader(shader);
  surface->getCanvas()->drawRect(surface_rect, paint);

  // Return the surface's contents as an SkImage.
  return surface->makeImageSnapshot();
}

bool ColorConversionSkFilterCache::UseToneCurve(sk_sp<SkImage> image) {
  DCHECK(image);
  auto* image_sk_color_space = image->colorSpace();
  if (!image_sk_color_space) {
    return false;
  }
  gfx::ColorSpace image_color_space(*image_sk_color_space);
  switch (image_color_space.GetTransferID()) {
    case ColorSpace::TransferID::PQ:
    case ColorSpace::TransferID::HLG:
      return true;
    default:
      return false;
  }
}

sk_sp<SkImage> ColorConversionSkFilterCache::ApplyToneCurve(
    sk_sp<SkImage> image,
    absl::optional<HDRMetadata> src_hdr_metadata,
    float dst_sdr_max_luminance_nits,
    float dst_max_luminance_relative,
    GrDirectContext* gr_context,
    skgpu::graphite::Recorder* graphite_recorder) {
  DCHECK(image);
  sk_sp<SkColorSpace> image_sk_color_space = image->refColorSpace();
  DCHECK(image_sk_color_space);
  ColorSpace image_color_space(*image_sk_color_space);
  DCHECK(image_color_space.GetTransferID() == ColorSpace::TransferID::PQ ||
         image_color_space.GetTransferID() == ColorSpace::TransferID::HLG);

  SkImageInfo image_info =
      SkImageInfo::Make(image->dimensions(),
                        SkColorInfo(kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                                    image_sk_color_space));
  sk_sp<SkSurface> surface =
      MakeSurfaceForResult(image_info, gr_context, graphite_recorder);
  if (!surface) {
    DLOG(ERROR) << "Failed to create SkSurface color conversion.";
    return nullptr;
  }

  ColorSpace target_color_space(ColorSpace::PrimaryID::BT2020,
                                ColorSpace::TransferID::LINEAR_HDR);
  sk_sp<SkColorFilter> filter =
      Get(image_color_space, target_color_space,
          /*resource_offset=*/0, /*resource_multiplier=*/1,
          /*src_bit_depth=*/absl::nullopt, src_hdr_metadata,
          dst_sdr_max_luminance_nits, dst_max_luminance_relative);
  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  paint.setColorFilter(filter);
  SkSamplingOptions sampling_options(SkFilterMode::kNearest);
  surface->getCanvas()->drawImage(image,
                                  /*x=*/0, /*y=*/0, sampling_options, &paint);
  return surface->makeImageSnapshot()->reinterpretColorSpace(
      target_color_space.ToSkColorSpace());
}

}  // namespace gfx
