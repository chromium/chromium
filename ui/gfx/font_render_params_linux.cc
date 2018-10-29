// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include <fontconfig/fontconfig.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/containers/mru_cache.h"
#include "base/hash.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gfx/font.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/gfx/switches.h"

namespace gfx {

namespace {

int FontWeightToFCWeight(Font::Weight weight) {
  const int weight_number = static_cast<int>(weight);
  if (weight_number <= (static_cast<int>(Font::Weight::THIN) +
                        static_cast<int>(Font::Weight::EXTRA_LIGHT)) /
                           2)
    return FC_WEIGHT_THIN;
  else if (weight_number <= (static_cast<int>(Font::Weight::EXTRA_LIGHT) +
                             static_cast<int>(Font::Weight::LIGHT)) /
                                2)
    return FC_WEIGHT_ULTRALIGHT;
  else if (weight_number <= (static_cast<int>(Font::Weight::LIGHT) +
                             static_cast<int>(Font::Weight::NORMAL)) /
                                2)
    return FC_WEIGHT_LIGHT;
  else if (weight_number <= (static_cast<int>(Font::Weight::NORMAL) +
                             static_cast<int>(Font::Weight::MEDIUM)) /
                                2)
    return FC_WEIGHT_NORMAL;
  else if (weight_number <= (static_cast<int>(Font::Weight::MEDIUM) +
                             static_cast<int>(Font::Weight::SEMIBOLD)) /
                                2)
    return FC_WEIGHT_MEDIUM;
  else if (weight_number <= (static_cast<int>(Font::Weight::SEMIBOLD) +
                             static_cast<int>(Font::Weight::BOLD)) /
                                2)
    return FC_WEIGHT_DEMIBOLD;
  else if (weight_number <= (static_cast<int>(Font::Weight::BOLD) +
                             static_cast<int>(Font::Weight::EXTRA_BOLD)) /
                                2)
    return FC_WEIGHT_BOLD;
  else if (weight_number <= (static_cast<int>(Font::Weight::EXTRA_BOLD) +
                             static_cast<int>(Font::Weight::BLACK)) /
                                2)
    return FC_WEIGHT_ULTRABOLD;
  else
    return FC_WEIGHT_BLACK;
}

// A device scale factor used to determine if subpixel positioning
// should be used.
float device_scale_factor_ = 1.0f;

// Number of recent GetFontRenderParams() results to cache.
const size_t kCacheSize = 256;

// Cached result from a call to GetFontRenderParams().
struct QueryResult {
  QueryResult(const FontRenderParams& params, const std::string& family)
      : params(params),
        family(family) {
  }
  ~QueryResult() {}

  FontRenderParams params;
  std::string family;
};

// Keyed by hashes of FontRenderParamQuery structs from
// HashFontRenderParamsQuery().
typedef base::MRUCache<uint32_t, QueryResult> Cache;

// A cache and the lock that must be held while accessing it.
// GetFontRenderParams() is called by both the UI thread and the sandbox IPC
// thread.
struct SynchronizedCache {
  SynchronizedCache() : cache(kCacheSize) {}

  base::Lock lock;
  Cache cache;
};

base::LazyInstance<SynchronizedCache>::Leaky g_synchronized_cache =
    LAZY_INSTANCE_INITIALIZER;

// Converts Fontconfig FC_HINT_STYLE to FontRenderParams::Hinting.
FontRenderParams::Hinting ConvertFontconfigHintStyle(int hint_style) {
  switch (hint_style) {
    case FC_HINT_SLIGHT: return FontRenderParams::HINTING_SLIGHT;
    case FC_HINT_MEDIUM: return FontRenderParams::HINTING_MEDIUM;
    case FC_HINT_FULL:   return FontRenderParams::HINTING_FULL;
    default:             return FontRenderParams::HINTING_NONE;
  }
}

// Converts Fontconfig FC_RGBA to FontRenderParams::SubpixelRendering.
FontRenderParams::SubpixelRendering ConvertFontconfigRgba(int rgba) {
  switch (rgba) {
    case FC_RGBA_RGB:  return FontRenderParams::SUBPIXEL_RENDERING_RGB;
    case FC_RGBA_BGR:  return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    case FC_RGBA_VRGB: return FontRenderParams::SUBPIXEL_RENDERING_VRGB;
    case FC_RGBA_VBGR: return FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    default:           return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }
}

// Queries Fontconfig for rendering settings and updates |params_out| and
// |family_out| (if non-NULL). Returns false on failure.
bool QueryFontconfig(const FontRenderParamsQuery& query,
                     FontRenderParams* params_out,
                     std::string* family_out) {
  struct FcPatternDeleter {
    void operator()(FcPattern* ptr) const { FcPatternDestroy(ptr); }
  };
  typedef std::unique_ptr<FcPattern, FcPatternDeleter> ScopedFcPattern;

  ScopedFcPattern query_pattern(FcPatternCreate());
  CHECK(query_pattern);

  FcPatternAddBool(query_pattern.get(), FC_SCALABLE, FcTrue);

  for (auto it = query.families.begin(); it != query.families.end(); ++it) {
    FcPatternAddString(query_pattern.get(),
        FC_FAMILY, reinterpret_cast<const FcChar8*>(it->c_str()));
  }
  if (query.pixel_size > 0)
    FcPatternAddDouble(query_pattern.get(), FC_PIXEL_SIZE, query.pixel_size);
  if (query.point_size > 0)
    FcPatternAddInteger(query_pattern.get(), FC_SIZE, query.point_size);
  if (query.style >= 0) {
    FcPatternAddInteger(query_pattern.get(), FC_SLANT,
        (query.style & Font::ITALIC) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
  }
  if (query.weight != Font::Weight::INVALID) {
    FcPatternAddInteger(query_pattern.get(), FC_WEIGHT,
                        FontWeightToFCWeight(query.weight));
  }

  FcConfigSubstitute(NULL, query_pattern.get(), FcMatchPattern);
  FcDefaultSubstitute(query_pattern.get());

  ScopedFcPattern result_pattern;
  if (query.is_empty()) {
    // If the query was empty, call FcConfigSubstituteWithPat() to get a
    // non-family- or size-specific configuration so it can be used as the
    // default.
    result_pattern.reset(FcPatternDuplicate(query_pattern.get()));
    if (!result_pattern)
      return false;
    FcPatternDel(result_pattern.get(), FC_FAMILY);
    FcPatternDel(result_pattern.get(), FC_PIXEL_SIZE);
    FcPatternDel(result_pattern.get(), FC_SIZE);
    FcConfigSubstituteWithPat(NULL, result_pattern.get(), query_pattern.get(),
                              FcMatchFont);
  } else {
    FcResult result;
    result_pattern.reset(FcFontMatch(NULL, query_pattern.get(), &result));
    if (!result_pattern)
      return false;
  }
  DCHECK(result_pattern);

  if (family_out) {
    FcChar8* family = NULL;
    FcPatternGetString(result_pattern.get(), FC_FAMILY, 0, &family);
    if (family)
      family_out->assign(reinterpret_cast<const char*>(family));
  }

  if (params_out) {
    FcBool fc_antialias = 0;
    if (FcPatternGetBool(result_pattern.get(), FC_ANTIALIAS, 0,
                         &fc_antialias) == FcResultMatch) {
      params_out->antialiasing = fc_antialias;
    }

    FcBool fc_autohint = 0;
    if (FcPatternGetBool(result_pattern.get(), FC_AUTOHINT, 0, &fc_autohint) ==
        FcResultMatch) {
      params_out->autohinter = fc_autohint;
    }

    FcBool fc_bitmap = 0;
    if (FcPatternGetBool(result_pattern.get(), FC_EMBEDDED_BITMAP, 0,
                         &fc_bitmap) ==
        FcResultMatch) {
      params_out->use_bitmaps = fc_bitmap;
    }

    FcBool fc_hinting = 0;
    if (FcPatternGetBool(result_pattern.get(), FC_HINTING, 0, &fc_hinting) ==
        FcResultMatch) {
      int fc_hint_style = FC_HINT_NONE;
      if (fc_hinting) {
        FcPatternGetInteger(
            result_pattern.get(), FC_HINT_STYLE, 0, &fc_hint_style);
      }
      params_out->hinting = ConvertFontconfigHintStyle(fc_hint_style);
    }

    int fc_rgba = FC_RGBA_NONE;
    if (FcPatternGetInteger(result_pattern.get(), FC_RGBA, 0, &fc_rgba) ==
        FcResultMatch)
      params_out->subpixel_rendering = ConvertFontconfigRgba(fc_rgba);
  }

  return true;
}

// Serialize |query| into a string and hash it to a value suitable for use as a
// cache key.
uint32_t HashFontRenderParamsQuery(const FontRenderParamsQuery& query) {
  return base::Hash(base::StringPrintf(
      "%d|%d|%d|%d|%s|%f", query.pixel_size, query.point_size, query.style,
      static_cast<int>(query.weight),
      base::JoinString(query.families, ",").c_str(),
      query.device_scale_factor));
}

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  FontRenderParamsQuery actual_query(query);
  if (actual_query.device_scale_factor == 0)
    actual_query.device_scale_factor = device_scale_factor_;

  const uint32_t hash = HashFontRenderParamsQuery(actual_query);
  SynchronizedCache* synchronized_cache = g_synchronized_cache.Pointer();

  {
    // Try to find a cached result so Fontconfig doesn't need to be queried.
    base::AutoLock lock(synchronized_cache->lock);
    Cache::const_iterator it = synchronized_cache->cache.Get(hash);
    if (it != synchronized_cache->cache.end()) {
      DVLOG(1) << "Returning cached params for " << hash;
      const QueryResult& result = it->second;
      if (family_out)
        *family_out = result.family;
      return result.params;
    }
  }

  DVLOG(1) << "Computing params for " << hash;
  if (family_out)
    family_out->clear();

  // Start with the delegate's settings, but let Fontconfig have the final say.
  FontRenderParams params;
  const LinuxFontDelegate* delegate = LinuxFontDelegate::instance();
  if (delegate)
    params = delegate->GetDefaultFontRenderParams();
  QueryFontconfig(actual_query, &params, family_out);
  if (!params.antialiasing) {
    // Cairo forces full hinting when antialiasing is disabled, since anything
    // less than that looks awful; do the same here. Requesting subpixel
    // rendering or positioning doesn't make sense either.
    params.hinting = FontRenderParams::HINTING_FULL;
    params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;
    params.subpixel_positioning = false;
  } else if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kDisableFontSubpixelPositioning)) {
#if !defined(OS_CHROMEOS)
    params.subpixel_positioning = actual_query.device_scale_factor > 1.0f;
#else
    // We want to enable subpixel positioning for fractional dsf.
    params.subpixel_positioning =
        std::abs(std::round(actual_query.device_scale_factor) -
                 actual_query.device_scale_factor) >
        std::numeric_limits<float>::epsilon();
#endif  // !defined(OS_CHROMEOS)

    // To enable subpixel positioning, we need to disable hinting.
    if (params.subpixel_positioning)
      params.hinting = FontRenderParams::HINTING_NONE;
  }

  // Use the first family from the list if Fontconfig didn't suggest a family.
  if (family_out && family_out->empty() && !actual_query.families.empty())
    *family_out = actual_query.families[0];

  {
    // Store the result. It's fine if this overwrites a result that was cached
    // by a different thread in the meantime; the values should be identical.
    base::AutoLock lock(synchronized_cache->lock);
    synchronized_cache->cache.Put(hash,
        QueryResult(params, family_out ? *family_out : std::string()));
  }

  return params;
}

void ClearFontRenderParamsCacheForTest() {
  SynchronizedCache* synchronized_cache = g_synchronized_cache.Pointer();
  base::AutoLock lock(synchronized_cache->lock);
  synchronized_cache->cache.Clear();
}

float GetFontRenderParamsDeviceScaleFactor() {
  return device_scale_factor_;
}

void SetFontRenderParamsDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

}  // namespace gfx
