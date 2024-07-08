// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/fontconfig_util.h"

#include <fontconfig/fontconfig.h>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/font_render_params.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#endif

namespace gfx {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr base::FilePath::CharType kGoogleSansVariablePath[] =
    FILE_PATH_LITERAL("/usr/share/fonts/google-sans/variable");
constexpr base::FilePath::CharType kGoogleSansStaticPath[] =
    FILE_PATH_LITERAL("/usr/share/fonts/google-sans/static");
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This should match `imageloader::kImageloaderMountBase` from
// //third_party/cros_system_api/constants/imageloader.h.
constexpr base::FilePath::CharType kImageloaderMountBase[] =
    FILE_PATH_LITERAL("/run/imageloader/");
#endif

// A singleton class to wrap a global font-config configuration. The
// configuration reference counter is incremented to avoid the deletion of the
// structure while being used. This class is single-threaded and should only be
// used on the UI-Thread.
class GFX_EXPORT GlobalFontConfig {
 public:
  GlobalFontConfig() {
    TRACE_EVENT0("ui", "GlobalFontConfig::GlobalFontConfig");
    SCOPED_UMA_HISTOGRAM_TIMER("Startup.InitializeFontConfigDuration");

    // Without this call, the FontConfig library gets implicitly initialized
    // on the first call to FontConfig. Since it's not safe to initialize it
    // concurrently from multiple threads, we explicitly initialize it here
    // to prevent races when there are multiple renderer's querying the library:
    // http://crbug.com/404311
    // Note that future calls to FcInit() are safe no-ops per the FontConfig
    // interface.
    FcInit();

    // Increment the reference counter to avoid the config to be deleted while
    // being used (see http://crbug.com/1004254).
    fc_config_ = FcConfigGetCurrent();
    FcConfigReference(fc_config_);
#if BUILDFLAG(IS_CHROMEOS)
    // TODO(b/268691415): Leave until M119 when all builds have the variable
    // font.
    if (base::PathExists(base::FilePath(kGoogleSansVariablePath))) {
      const FcChar8* kVariableFontPath =
          reinterpret_cast<const FcChar8*>(kGoogleSansVariablePath);
      // Adds the folder to the available fonts in the application. Returns
      // false only when fonts cannot be added due to "allocation failure".
      // https://www.freedesktop.org/software/fontconfig/fontconfig-devel/fcconfigappfontadddir.html
      CHECK(FcConfigAppFontAddDir(fc_config_, kVariableFontPath));
    } else {
      const FcChar8* kStaticFontPath =
          reinterpret_cast<const FcChar8*>(kGoogleSansStaticPath);
      CHECK(FcConfigAppFontAddDir(fc_config_, kStaticFontPath));
    }
#endif

    // Set rescan interval to 0 to disable re-scan. Re-scanning in the
    // background is a source of thread safety issues.
    // See in http://crbug.com/1004254.
    FcBool result = FcConfigSetRescanInterval(fc_config_, 0);
    DCHECK_EQ(result, FcTrue);
  }

  GlobalFontConfig(const GlobalFontConfig&) = delete;
  GlobalFontConfig& operator=(const GlobalFontConfig&) = delete;

  ~GlobalFontConfig() { FcConfigDestroy(fc_config_.ExtractAsDangling()); }

  // Retrieve the native font-config FcConfig pointer.
  FcConfig* Get() const {
    DCHECK_EQ(fc_config_, FcConfigGetCurrent());
    return fc_config_;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool AddAppFontDir(const base::FilePath& dir) {
    if (dir.ReferencesParent()) {
      // Possible path traversal.
      return false;
    }
    if (!base::FilePath(kImageloaderMountBase).IsParent(dir)) {
      // Not a DLC path.
      return false;
    }
    if (app_font_dirs_added_.contains(dir)) {
      // Added before.
      return false;
    }
    app_font_dirs_added_.insert(dir);

    // Points to memory owned by `dir`.
    const FcChar8* dir_fcstring =
        reinterpret_cast<const FcChar8*>(dir.value().c_str());

    // Adds the folder to the available fonts in the application. Returns
    // false only when fonts cannot be added due to "allocation failure".
    // https://www.freedesktop.org/software/fontconfig/fontconfig-devel/fcconfigappfontadddir.html
    return FcConfigAppFontAddDir(fc_config_, dir_fcstring);
  }
#endif

  // Override the font-config configuration.
  void OverrideForTesting(FcConfig* config) {
    FcConfigSetCurrent(config);
    fc_config_ = config;
  }

  // Retrieve the global font-config configuration.
  static GlobalFontConfig* GetInstance() {
    static base::NoDestructor<GlobalFontConfig> fontconfig;
    return fontconfig.get();
  }

 private:
  raw_ptr<FcConfig> fc_config_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::flat_set<base::FilePath> app_font_dirs_added_;
#endif
};

// Converts Fontconfig FC_HINT_STYLE to FontRenderParams::Hinting.
FontRenderParams::Hinting ConvertFontconfigHintStyle(int hint_style) {
  switch (hint_style) {
    case FC_HINT_SLIGHT:
      return FontRenderParams::HINTING_SLIGHT;
    case FC_HINT_MEDIUM:
      return FontRenderParams::HINTING_MEDIUM;
    case FC_HINT_FULL:
      return FontRenderParams::HINTING_FULL;
    default:
      return FontRenderParams::HINTING_NONE;
  }
}

// Converts Fontconfig FC_RGBA to FontRenderParams::SubpixelRendering.
FontRenderParams::SubpixelRendering ConvertFontconfigRgba(int rgba) {
  switch (rgba) {
    case FC_RGBA_RGB:
      return FontRenderParams::SUBPIXEL_RENDERING_RGB;
    case FC_RGBA_BGR:
      return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    case FC_RGBA_VRGB:
      return FontRenderParams::SUBPIXEL_RENDERING_VRGB;
    case FC_RGBA_VBGR:
      return FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    default:
      return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }
}

// Extracts a string property from a font-config pattern (e.g. FcPattern).
std::string GetFontConfigPropertyAsString(FcPattern* pattern,
                                          const char* property) {
  FcChar8* text = nullptr;
  if (FcPatternGetString(pattern, property, 0, &text) != FcResultMatch ||
      text == nullptr) {
    return std::string();
  }
  return std::string(reinterpret_cast<const char*>(text));
}

// Extracts an integer property from a font-config pattern (e.g. FcPattern).
int GetFontConfigPropertyAsInt(FcPattern* pattern,
                               const char* property,
                               int default_value) {
  int value = -1;
  if (FcPatternGetInteger(pattern, property, 0, &value) != FcResultMatch) {
    return default_value;
  }
  return value;
}

// Extracts an boolean property from a font-config pattern (e.g. FcPattern).
bool GetFontConfigPropertyAsBool(FcPattern* pattern, const char* property) {
  FcBool value = FcFalse;
  if (FcPatternGetBool(pattern, property, 0, &value) != FcResultMatch) {
    return false;
  }
  return value != FcFalse;
}

}  // namespace

void InitializeGlobalFontConfigAsync() {
  if (base::ThreadPoolInstance::Get()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce([]() { GlobalFontConfig::GetInstance(); }));
  } else {
    GlobalFontConfig::GetInstance();
  }
}

FcConfig* GetGlobalFontConfig() {
  return GlobalFontConfig::GetInstance()->Get();
}

void OverrideGlobalFontConfigForTesting(FcConfig* config) {
  return GlobalFontConfig::GetInstance()->OverrideForTesting(config);
}

std::string GetFontName(FcPattern* pattern) {
  return GetFontConfigPropertyAsString(pattern, FC_FAMILY);
}

std::string GetFilename(FcPattern* pattern) {
  return GetFontConfigPropertyAsString(pattern, FC_FILE);
}

base::FilePath GetFontPath(FcPattern* pattern) {
  std::string filename = GetFilename(pattern);

  // Obtains the system root directory in 'config' if available. All files
  // (including file properties in patterns) obtained from this 'config' are
  // relative to this system root directory.
  const char* sysroot =
      reinterpret_cast<const char*>(FcConfigGetSysRoot(nullptr));
  if (!sysroot) {
    return base::FilePath(filename);
  }

  // Paths may be specified with a heading slash (e.g.
  // /test_fonts/DejaVuSans.ttf).
  if (!filename.empty() && base::FilePath::IsSeparator(filename[0])) {
    filename = filename.substr(1);
  }

  if (filename.empty()) {
    return base::FilePath();
  }

  return base::FilePath(sysroot).Append(filename);
}

int GetFontTtcIndex(FcPattern* pattern) {
  return GetFontConfigPropertyAsInt(pattern, FC_INDEX, 0);
}

bool IsFontBold(FcPattern* pattern) {
  int weight = GetFontConfigPropertyAsInt(pattern, FC_WEIGHT, FC_WEIGHT_NORMAL);
  return weight >= FC_WEIGHT_BOLD;
}

bool IsFontItalic(FcPattern* pattern) {
  int slant = GetFontConfigPropertyAsInt(pattern, FC_SLANT, FC_SLANT_ROMAN);
  return slant != FC_SLANT_ROMAN;
}

bool IsFontScalable(FcPattern* pattern) {
  return GetFontConfigPropertyAsBool(pattern, FC_SCALABLE);
}

std::string GetFontFormat(FcPattern* pattern) {
  return GetFontConfigPropertyAsString(pattern, FC_FONTFORMAT);
}

void GetFontRenderParamsFromFcPattern(FcPattern* pattern,
                                      FontRenderParams* param_out) {
  FcBool fc_antialias = 0;
  if (FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &fc_antialias) ==
      FcResultMatch) {
    param_out->antialiasing = fc_antialias;
  }

  FcBool fc_autohint = 0;
  if (FcPatternGetBool(pattern, FC_AUTOHINT, 0, &fc_autohint) ==
      FcResultMatch) {
    param_out->autohinter = fc_autohint;
  }

  FcBool fc_bitmap = 0;
  if (FcPatternGetBool(pattern, FC_EMBEDDED_BITMAP, 0, &fc_bitmap) ==
      FcResultMatch) {
    param_out->use_bitmaps = fc_bitmap;
  }

  FcBool fc_hinting = 0;
  if (FcPatternGetBool(pattern, FC_HINTING, 0, &fc_hinting) == FcResultMatch) {
    int fc_hint_style = FC_HINT_NONE;
    if (fc_hinting) {
      FcPatternGetInteger(pattern, FC_HINT_STYLE, 0, &fc_hint_style);
    }
    param_out->hinting = ConvertFontconfigHintStyle(fc_hint_style);
  }

  int fc_rgba = FC_RGBA_NONE;
  if (FcPatternGetInteger(pattern, FC_RGBA, 0, &fc_rgba) == FcResultMatch) {
    param_out->subpixel_rendering = ConvertFontconfigRgba(fc_rgba);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool AddAppFontDir(const base::FilePath& dir) {
  return CHECK_DEREF(GlobalFontConfig::GetInstance()).AddAppFontDir(dir);
}
#endif

}  // namespace gfx
