// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_util_win.h"

#include <windows.h>

#include <wrl/client.h>

#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/win/direct_write.h"

namespace gfx {

namespace {
struct TextParameters {
  float contrast;
  float gamma;
};

TextParameters GetTextParameters() {
  static TextParameters text_parameters;
  static std::once_flag flag;
  std::call_once(flag, [&] {
    text_parameters.contrast = FontUtilWin::TextGammaContrast();
    text_parameters.gamma = SK_GAMMA_EXPONENT;
    // Only apply values from `IDWriteRenderingParams` if the user has
    // the appropriate registry keys set. Otherwise, `IDWriteRenderingParams`
    // values will use DirectWrite's default values, which do no match Skia's
    // defaults.
    base::win::RegKey key = FontUtilWin::GetTextSettingsRegistryKey();
    if (key.Valid()) {
      Microsoft::WRL::ComPtr<IDWriteFactory> factory;
      gfx::win::CreateDWriteFactory(&factory);
      if (factory) {
        // We only support the primary device currently.
        Microsoft::WRL::ComPtr<IDWriteRenderingParams> text_params;
        if (SUCCEEDED(factory->CreateRenderingParams(&text_params))) {
          text_parameters.contrast =
              FontUtilWin::ClampContrast(text_params->GetEnhancedContrast());
          text_parameters.gamma =
              FontUtilWin::ClampGamma(text_params->GetGamma());
        }
      }
    }
  });
  return text_parameters;
}
}  // namespace

// static
base::win::RegKey FontUtilWin::GetTextSettingsRegistryKey(REGSAM access) {
  DISPLAY_DEVICE display_device = {sizeof(DISPLAY_DEVICE)};
  for (int i = 0; EnumDisplayDevices(nullptr, i, &display_device, 0); ++i) {
    // TODO(scottmg): We only support the primary device currently.
    if (display_device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      base::FilePath trimmed =
          base::FilePath(display_device.DeviceName).BaseName();
      return base::win::RegKey{
          HKEY_CURRENT_USER,
          (L"SOFTWARE\\Microsoft\\Avalon.Graphics\\" + trimmed.value()).c_str(),
          access};
    }
  }
  return base::win::RegKey{};
}

// static
float FontUtilWin::ClampContrast(float value) {
  return std::clamp(value, SkSurfaceProps::kMinContrastInclusive,
                    SkSurfaceProps::kMaxContrastInclusive);
}

// static
float FontUtilWin::ClampGamma(float value) {
  // Handle the exclusive max by subtracting espilon.
  return std::clamp(value, SkSurfaceProps::kMinGammaInclusive,
                    SkSurfaceProps::kMaxGammaExclusive -
                        std::numeric_limits<float>::epsilon());
}

// static
float FontUtilWin::GetContrastFromRegistry() {
  return GetTextParameters().contrast;
}

// static
float FontUtilWin::GetGammaFromRegistry() {
  return GetTextParameters().gamma;
}

// static
float FontUtilWin::TextGammaContrast() {
  if (base::FeatureList::IsEnabled(features::kIncreaseWindowsTextContrast)) {
    // On Windows, SK_GAMMA_CONTRAST is currently 0.5. This flag increases it
    // to 1.0.
    return 1.0f;
  } else {
    return SK_GAMMA_CONTRAST;
  }
}

}  // namespace gfx
