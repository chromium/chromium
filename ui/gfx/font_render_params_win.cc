// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include <windows.h>

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/notimplemented.h"
#include "base/win/registry.h"
#include "skia/ext/legacy_display_globals.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/font_util_win.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

namespace {

FontRenderParams::SubpixelRendering GetSubpixelRenderingGeometry() {
  DWORD pixel_structure;
  base::win::RegKey key = FontUtilWin::GetTextSettingsRegistryKey();
  if (key.Valid() &&
      key.ReadValueDW(L"PixelStructure", &pixel_structure) == ERROR_SUCCESS) {
    switch (pixel_structure) {
      case 0:
        return FontRenderParams::SUBPIXEL_RENDERING_NONE;
      case 1:
        return FontRenderParams::SUBPIXEL_RENDERING_RGB;
      case 2:
        return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    }
    // TODO(kschmi): Determine usage of this fallback and remove if it's not
    // hit.
    return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }

  UINT structure = 0;
  if (SystemParametersInfo(SPI_GETFONTSMOOTHINGORIENTATION, 0, &structure, 0)) {
    switch (structure) {
      case FE_FONTSMOOTHINGORIENTATIONRGB:
        return FontRenderParams::SUBPIXEL_RENDERING_RGB;
      case FE_FONTSMOOTHINGORIENTATIONBGR:
        return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    }
  }

  // No explicit ClearType settings, default to none.
  return FontRenderParams::SUBPIXEL_RENDERING_NONE;
}

// Caches font render params and updates them on system notifications.
class CachedFontRenderParams {
 public:
  static CachedFontRenderParams* GetInstance() {
    return base::Singleton<CachedFontRenderParams>::get();
  }

  CachedFontRenderParams(const CachedFontRenderParams&) = delete;
  CachedFontRenderParams& operator=(const CachedFontRenderParams&) = delete;

  const FontRenderParams& GetParams() {
    if (params_)
      return *params_;

    params_ = std::make_unique<FontRenderParams>();
    params_->antialiasing = false;
    params_->subpixel_positioning = false;
    params_->autohinter = false;
    params_->use_bitmaps = false;
    params_->hinting = FontRenderParams::HINTING_MEDIUM;
    params_->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;

    BOOL enabled = false;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) && enabled) {
      params_->antialiasing = true;
      params_->subpixel_positioning = true;

      UINT type = 0;
      if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0) &&
          type == FE_FONTSMOOTHINGCLEARTYPE) {
        params_->subpixel_rendering = GetSubpixelRenderingGeometry();
      }
    }

    if (base::FeatureList::IsEnabled(
            features::kUseGammaContrastRegistrySettings)) {
      params_->text_contrast = FontUtilWin::GetContrastFromRegistry();
      params_->text_gamma = FontUtilWin::GetGammaFromRegistry();
    } else {
      params_->text_contrast = SK_GAMMA_CONTRAST;
      params_->text_gamma = SK_GAMMA_EXPONENT;
    }

    skia::LegacyDisplayGlobals::SetCachedParams(
        FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
            params_->subpixel_rendering),
        params_->text_contrast, params_->text_gamma);

    hwnd_subscription_ =
        gfx::SingletonHwnd::GetInstance()->RegisterCallback(base::BindRepeating(
            &CachedFontRenderParams::OnWndProc, base::Unretained(this)));
    return *params_;
  }

  void Reset() {
    params_.reset();
    hwnd_subscription_.reset();
  }

 private:
  friend struct base::DefaultSingletonTraits<CachedFontRenderParams>;

  CachedFontRenderParams() {}
  ~CachedFontRenderParams() {}

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_SETTINGCHANGE) {
      // TODO(khushalsagar): This should trigger an update to the
      // renderer and gpu processes, where the params are cached.
      if (wparam == SPI_GETCLIENTAREAANIMATION &&
          base::features::IsReducePPMsEnabled()) {
        Animation::UpdatePrefersReducedMotion();
      }
      params_.reset();
      hwnd_subscription_.reset();
    }
  }

  std::unique_ptr<FontRenderParams> params_;
  std::optional<base::CallbackListSubscription> hwnd_subscription_;
};

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  if (family_out)
    NOTIMPLEMENTED();
  // Customized font rendering settings are not supported, only defaults.
  return CachedFontRenderParams::GetInstance()->GetParams();
}

void ClearFontRenderParamsCacheForTest() {
  CachedFontRenderParams::GetInstance()->Reset();
}

float GetFontRenderParamsDeviceScaleFactor() {
  return 1.;
}

}  // namespace gfx
