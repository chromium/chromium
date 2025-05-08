// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_WUC_BACKDROP_H_
#define UI_GFX_WIN_WUC_BACKDROP_H_

#include <windows.h>

#include <windows.ui.composition.desktop.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>
#include <wrl/client.h>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {

// Creates a small Windows UI Composition (WUC) visual tree that serves as the
// backdrop for any HWND that is specified.
class COMPONENT_EXPORT(GFX) WUCBackdrop {
 public:
  WUCBackdrop(HWND hwnd);
  ~WUCBackdrop();

  // Takes an SkColor and applies it to the backdrop sprite visual.
  void UpdateBackdropColor(SkColor color);

  // Load the CoreMessaging library and `function_name`. This function contains
  // a blocking call.
  static FARPROC LoadCoreMessagingFunction(const char* function_name);

 private:
  // Represents the target of the WUC tree and retains a link to the root
  // visual.
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget>
      desktop_window_target_;
  // Sprite visuals take on color. This will be the leaf of the WUC tree and
  // serve as the backdrop. Maintain a reference to the visual so that the
  // visual is updated with browser theme.
  Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ISpriteVisual>
      backdrop_sprite_visual_;
  // The color brush that will be used to color the backdrop sprite visual.
  Microsoft::WRL::ComPtr<ABI::Windows::UI::Composition::ICompositionColorBrush>
      solid_color_brush_;
};

}  // namespace gfx

#endif  // UI_GFX_WIN_WUC_BACKDROP_H_
