// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_FONTCONFIG_UTIL_H_
#define UI_GFX_LINUX_FONTCONFIG_UTIL_H_

#include <fontconfig/fontconfig.h>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "ui/gfx/font_render_params.h"

namespace gfx {

struct FcPatternDeleter {
  void operator()(FcPattern* ptr) const { FcPatternDestroy(ptr); }
};
using ScopedFcPattern = std::unique_ptr<FcPattern, FcPatternDeleter>;

// Initializes FontConfig on a worker thread if a thread pool instance is
// available, otherwise initializes FontConfig in a blocking fashion on the
// calling thread.  If this function is not called, the first call to
// GetGlobalFontConfig() will implicitly initialize FontConfig.  Can be called
// on any thread.
COMPONENT_EXPORT(GFX) void InitializeGlobalFontConfigAsync();

// Retrieve the global font config. Must be called on the main thread.
COMPONENT_EXPORT(GFX) FcConfig* GetGlobalFontConfig();
COMPONENT_EXPORT(GFX) void OverrideGlobalFontConfigForTesting(FcConfig* config);

// FcPattern accessor wrappers.
COMPONENT_EXPORT(GFX) std::string GetFontName(FcPattern* pattern);
COMPONENT_EXPORT(GFX) std::string GetFilename(FcPattern* pattern);
COMPONENT_EXPORT(GFX) int GetFontTtcIndex(FcPattern* pattern);
COMPONENT_EXPORT(GFX) bool IsFontBold(FcPattern* pattern);
COMPONENT_EXPORT(GFX) bool IsFontItalic(FcPattern* pattern);
COMPONENT_EXPORT(GFX) bool IsFontScalable(FcPattern* pattern);
COMPONENT_EXPORT(GFX) std::string GetFontFormat(FcPattern* pattern);

// Return the path of the font. Relative to the sysroot config specified in the
// font config (see: FcConfigGetSysRoot(...)).
COMPONENT_EXPORT(GFX) base::FilePath GetFontPath(FcPattern* pattern);

// Returns the appropriate parameters for rendering the font represented by the
// font config pattern.
COMPONENT_EXPORT(GFX)
void GetFontRenderParamsFromFcPattern(FcPattern* pattern,
                                      FontRenderParams* param_out);

#if BUILDFLAG(IS_CHROMEOS)
// Adds a given directory to the available fonts in the application.
// Directory must start with `/run/imageloader/` (guaranteed by DLC).
// Returns whether the fonts were added or not. Will not add the same directory
// more than once.
COMPONENT_EXPORT(GFX) bool AddAppFontDir(const base::FilePath& dir);
#endif

}  // namespace gfx

#endif  // UI_GFX_LINUX_FONTCONFIG_UTIL_H_
