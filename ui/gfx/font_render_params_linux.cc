// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include <fontconfig/fontconfig.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/containers/lru_cache.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/linux/fontconfig_util.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

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
      : params(params), family(family) {}
  ~QueryResult() {}

  FontRenderParams params;
  std::string family;
};

// Keyed by hashes of FontRenderParamQuery structs from
// HashFontRenderParamsQuery().
typedef base::HashingLRUCache<std::string, QueryResult> Cache;

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

// Serialize |query| into a string value suitable for use as a cache key.
std::string GetFontRenderParamsQueryKey(const FontRenderParamsQuery& query) {
  return base::StringPrintf(
      "%d|%d|%d|%d|%s|%f", query.pixel_size, query.point_size, query.style,
      static_cast<int>(query.weight),
      base::JoinString(query.families, ",").c_str(), query.device_scale_factor);
}

}  // namespace

bool QueryFontconfig(const FontRenderParamsQuery& query,
                     FontRenderParams* params_out,
                     std::string* family_out) {
  TRACE_EVENT0("fonts", "gfx::QueryFontconfig");

  ScopedFcPattern query_pattern(FcPatternCreate());
  CHECK(query_pattern);

  FcPatternAddBool(query_pattern.get(), FC_SCALABLE, FcTrue);

  for (auto it = query.families.begin(); it != query.families.end(); ++it) {
    FcPatternAddString(query_pattern.get(), FC_FAMILY,
                       reinterpret_cast<const FcChar8*>(it->c_str()));
  }
  if (query.pixel_size > 0)
    FcPatternAddDouble(query_pattern.get(), FC_PIXEL_SIZE, query.pixel_size);
  if (query.point_size > 0)
    FcPatternAddInteger(query_pattern.get(), FC_SIZE, query.point_size);
  if (query.style >= 0) {
    FcPatternAddInteger(
        query_pattern.get(), FC_SLANT,
        (query.style & Font::ITALIC) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
  }
  if (query.weight != Font::Weight::INVALID) {
    FcPatternAddInteger(query_pattern.get(), FC_WEIGHT,
                        FontWeightToFCWeight(query.weight));
  }

  FcConfig* config = GetGlobalFontConfig();
  FcConfigSubstitute(config, query_pattern.get(), FcMatchPattern);
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
    FcConfigSubstituteWithPat(config, result_pattern.get(), query_pattern.get(),
                              FcMatchFont);
  } else {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("fonts"), "FcFontMatch");
    FcResult result;
    result_pattern.reset(FcFontMatch(config, query_pattern.get(), &result));
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

  if (params_out)
    GetFontRenderParamsFromFcPattern(result_pattern.get(), params_out);

  return true;
}

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  TRACE_EVENT0("fonts", "gfx::GetFontRenderParams");

  FontRenderParamsQuery actual_query(query);
  if (actual_query.device_scale_factor == 0)
    actual_query.device_scale_factor = device_scale_factor_;

  std::string query_key = GetFontRenderParamsQueryKey(actual_query);
  SynchronizedCache* synchronized_cache = g_synchronized_cache.Pointer();

  {
    // Try to find a cached result so Fontconfig doesn't need to be queried.
    base::AutoLock lock(synchronized_cache->lock);
    Cache::const_iterator it = synchronized_cache->cache.Get(query_key);
    if (it != synchronized_cache->cache.end()) {
      DVLOG(1) << "Returning cached params for " << query_key;
      const QueryResult& result = it->second;
      if (family_out)
        *family_out = result.family;
      return result.params;
    }
  }

  DVLOG(1) << "Computing params for " << query_key;
  if (family_out)
    family_out->clear();

  // Start with the delegate's settings, but let Fontconfig have the final say.
  FontRenderParams params;
#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    params = linux_ui->GetDefaultFontRenderParams();
  }
#endif
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
#if BUILDFLAG(IS_CHROMEOS)
    // We want to enable subpixel positioning for fractional dsf.
    params.subpixel_positioning =
        std::abs(std::round(actual_query.device_scale_factor) -
                 actual_query.device_scale_factor) >
        std::numeric_limits<float>::epsilon();
#else
    params.subpixel_positioning = actual_query.device_scale_factor > 1.0f;
#endif  // BUILDFLAG(IS_CHROMEOS)

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
    synchronized_cache->cache.Put(
        query_key,
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
