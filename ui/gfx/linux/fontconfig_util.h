// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_FONTCONFIG_UTIL_H_
#define UI_GFX_LINUX_FONTCONFIG_UTIL_H_

#include <fontconfig/fontconfig.h>

#include "base/files/file_path.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

struct FcPatternDeleter {
  void operator()(FcPattern* ptr) const { FcPatternDestroy(ptr); }
};
using ScopedFcPattern = std::unique_ptr<FcPattern, FcPatternDeleter>;

// Retrieve the global font config. Must be called on the main thread.
GFX_EXPORT FcConfig* GetGlobalFontConfig();
GFX_EXPORT void OverrideGlobalFontConfigForTesting(FcConfig* config);

// FcPattern accessor wrappers.
GFX_EXPORT std::string GetFontName(FcPattern* pattern);
GFX_EXPORT std::string GetFilename(FcPattern* pattern);
GFX_EXPORT int GetFontTtcIndex(FcPattern* pattern);
GFX_EXPORT bool IsFontBold(FcPattern* pattern);
GFX_EXPORT bool IsFontItalic(FcPattern* pattern);
GFX_EXPORT bool IsFontScalable(FcPattern* pattern);
GFX_EXPORT std::string GetFontFormat(FcPattern* pattern);

// Return the path of the font. Relative to the sysroot config specified in the
// font config (see: FcConfigGetSysRoot(...)).
GFX_EXPORT base::FilePath GetFontPath(FcPattern* pattern);

// Returns the appropriate parameters for rendering the font represented by the
// font config pattern.
GFX_EXPORT void GetFontRenderParamsFromFcPattern(FcPattern* pattern,
                                                 FontRenderParams* param_out);

}  // namespace gfx

#endif  // UI_GFX_LINUX_FONTCONFIG_UTIL_H_