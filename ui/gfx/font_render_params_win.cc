// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/win/registry.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {

namespace {

FontRenderParams::SubpixelRendering GetSubpixelRenderingGeometry() {
  DISPLAY_DEVICE display_device = {sizeof(DISPLAY_DEVICE)};
  for (int i = 0; EnumDisplayDevices(nullptr, i, &display_device, 0); ++i) {
    // TODO(scottmg): We only support the primary device currently.
    if (display_device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      base::FilePath trimmed =
          base::FilePath(display_device.DeviceName).BaseName();
      base::win::RegKey key(
          HKEY_LOCAL_MACHINE,
          (L"SOFTWARE\\Microsoft\\Avalon.Graphics\\" + trimmed.value()).c_str(),
          KEY_READ);
      DWORD structure;
      if (key.ReadValueDW(L"PixelStructure", &structure) == ERROR_SUCCESS) {
        switch (structure) {
          case 0:
            return FontRenderParams::SUBPIXEL_RENDERING_NONE;
          case 1:
            return FontRenderParams::SUBPIXEL_RENDERING_RGB;
          case 2:
            return FontRenderParams::SUBPIXEL_RENDERING_BGR;
        }
        return FontRenderParams::SUBPIXEL_RENDERING_NONE;
      }
      break;
    }
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
    singleton_hwnd_observer_ =
        std::make_unique<SingletonHwndObserver>(base::BindRepeating(
            &CachedFontRenderParams::OnWndProc, base::Unretained(this)));
    return *params_;
  }

 private:
  friend struct base::DefaultSingletonTraits<CachedFontRenderParams>;

  CachedFontRenderParams() {}
  ~CachedFontRenderParams() {}

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_SETTINGCHANGE) {
      // TODO(khushalsagar): This should trigger an update to the
      // renderer and gpu processes, where the params are cached.
      params_.reset();
      singleton_hwnd_observer_.reset(nullptr);
    }
  }

  std::unique_ptr<FontRenderParams> params_;
  std::unique_ptr<SingletonHwndObserver> singleton_hwnd_observer_;
};

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  if (family_out)
    NOTIMPLEMENTED();
  // Customized font rendering settings are not supported, only defaults.
  return CachedFontRenderParams::GetInstance()->GetParams();
}

float GetFontRenderParamsDeviceScaleFactor() {
  return 1.;
}

}  // namespace gfx
