// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_util_win.h"

#include <windows.h>
#include <wrl/client.h>

#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkTypes.h"
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
    Microsoft::WRL::ComPtr<IDWriteFactory> factory;
    gfx::win::CreateDWriteFactory(&factory);
    if (factory) {
      // We only support the primary device currently.
      Microsoft::WRL::ComPtr<IDWriteRenderingParams> textParams;
      if (SUCCEEDED(factory->CreateRenderingParams(&textParams))) {
        text_parameters.contrast = textParams->GetEnhancedContrast();
        text_parameters.gamma = textParams->GetGamma();
      } else {
        text_parameters.contrast = SK_GAMMA_CONTRAST;
        text_parameters.gamma = SK_GAMMA_EXPONENT;
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
float FontUtilWin::GetContrastFromRegistry() {
  return GetTextParameters().contrast;
}

// static
float FontUtilWin::GetGammaFromRegistry() {
  return GetTextParameters().gamma;
}

}  // namespace gfx
