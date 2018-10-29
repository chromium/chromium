// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/skia_vector_resource.h"

#include <map>

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/skia_vector_animation.h"

#if defined(OS_WIN)
#include "ui/display/win/dpi.h"
#endif

namespace aura_extra {
namespace {

// Cached vector graphics. Each resource is loaded and unzipped only once.
// TODO(malaykeshav): Investigate if this needs to be an MRU cache with a size
// limit as the usage increases.
using VectorAssetCache = std::map<int, scoped_refptr<cc::SkottieWrapper>>;
VectorAssetCache& GetVectorAssetCache() {
  static base::NoDestructor<VectorAssetCache> vector_graphic_cache;
  return *vector_graphic_cache;
}

}  // namespace

std::unique_ptr<gfx::SkiaVectorAnimation> GetVectorAnimationNamed(
    int resource_id) {
  auto found = GetVectorAssetCache().find(resource_id);
  if (found != GetVectorAssetCache().end())
    return std::make_unique<gfx::SkiaVectorAnimation>(found->second);

  auto& rb = ui::ResourceBundle::GetSharedInstance();
#if defined(OS_CHROMEOS)
  ui::ScaleFactor scale_factor_to_load = rb.GetMaxScaleFactor();
#elif defined(OS_WIN)
  ui::ScaleFactor scale_factor_to_load = display::win::GetDPIScale() > 1.25
                                             ? rb.GetMaxScaleFactor()
                                             : ui::SCALE_FACTOR_100P;
#else
  ui::ScaleFactor scale_factor_to_load = ui::SCALE_FACTOR_100P;
#endif
  // Clamp the scale factor to 2x. At most we will only be needing 2 versions
  // for a given file.
  if (scale_factor_to_load > ui::SCALE_FACTOR_200P)
    scale_factor_to_load = ui::SCALE_FACTOR_200P;

  auto compressed_raw_data =
      rb.GetRawDataResourceForScale(resource_id, scale_factor_to_load);
  auto* uncompressed_bytes = new base::RefCountedBytes(
      compression::GetUncompressedSize(compressed_raw_data));
  base::StringPiece uncompressed_str_piece(
      reinterpret_cast<const char*>(uncompressed_bytes->front()),
      uncompressed_bytes->size());

  TRACE_EVENT1("ui", "GetVectorAnimationNamed uncompress and parse",
               "zip size bytes", uncompressed_bytes->size());
  base::TimeTicks start_timestamp = base::TimeTicks::Now();
  CHECK(
      compression::GzipUncompress(compressed_raw_data, uncompressed_str_piece));

  auto skottie = base::MakeRefCounted<cc::SkottieWrapper>(uncompressed_bytes);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "UncompressAndParseSkiaVectorAsset",
      base::TimeTicks::Now() - start_timestamp,
      base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMilliseconds(50), 100);
  auto inserted = GetVectorAssetCache().emplace(resource_id, skottie);
  DCHECK(inserted.second);
  return std::make_unique<gfx::SkiaVectorAnimation>(inserted.first->second);
}

}  // namespace aura_extra
