// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include <windows.h>

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_set.h"
#include "ui/gfx/color_utils.h"

namespace ui {

void AddMixerForNativeColors(ColorProvider* provider) {
  // TODO(pkasting): Not clear whether this is really the set of interest.
  // Maybe there's some way to query colors used by UxTheme.dll, or maybe we
  // should be hardcoding a list of colors for system light/dark modes based on
  // reverse-engineering current Windows behavior.  Or maybe the union of all
  // these.
#define MAP(chrome, native) {chrome, color_utils::GetSysSkColor(native)}
  provider->AddMixer().AddSet(
      {kColorSetNative,
       {
           MAP(kColorNative3dDkShadow, COLOR_3DDKSHADOW),
           MAP(kColorNative3dLight, COLOR_3DLIGHT),
           MAP(kColorNativeActiveBorder, COLOR_ACTIVEBORDER),
           MAP(kColorNativeActiveCaption, COLOR_ACTIVECAPTION),
           MAP(kColorNativeAppWorkspace, COLOR_APPWORKSPACE),
           MAP(kColorNativeBackground, COLOR_BACKGROUND),
           MAP(kColorNativeBtnFace, COLOR_BTNFACE),
           MAP(kColorNativeBtnHighlight, COLOR_BTNHIGHLIGHT),
           MAP(kColorNativeBtnShadow, COLOR_BTNSHADOW),
           MAP(kColorNativeBtnText, COLOR_BTNTEXT),
           MAP(kColorNativeCaptionText, COLOR_CAPTIONTEXT),
           MAP(kColorNativeGradientActiveCaption, COLOR_GRADIENTACTIVECAPTION),
           MAP(kColorNativeGradientInactiveCaption,
               COLOR_GRADIENTINACTIVECAPTION),
           MAP(kColorNativeGrayText, COLOR_GRAYTEXT),
           MAP(kColorNativeHighlight, COLOR_HIGHLIGHT),
           MAP(kColorNativeHighlightText, COLOR_HIGHLIGHTTEXT),
           MAP(kColorNativeHotlight, COLOR_HOTLIGHT),
           MAP(kColorNativeInactiveBorder, COLOR_INACTIVEBORDER),
           MAP(kColorNativeInactiveCaption, COLOR_INACTIVECAPTION),
           MAP(kColorNativeInactiveCaptionText, COLOR_INACTIVECAPTIONTEXT),
           MAP(kColorNativeInfoBk, COLOR_INFOBK),
           MAP(kColorNativeInfoText, COLOR_INFOTEXT),
           MAP(kColorNativeMenu, COLOR_MENU),
           MAP(kColorNativeMenuBar, COLOR_MENUBAR),
           MAP(kColorNativeMenuHilight, COLOR_MENUHILIGHT),
           MAP(kColorNativeMenuText, COLOR_MENUTEXT),
           MAP(kColorNativeScrollbar, COLOR_SCROLLBAR),
           MAP(kColorNativeWindow, COLOR_WINDOW),
           MAP(kColorNativeWindowFrame, COLOR_WINDOWFRAME),
           MAP(kColorNativeWindowText, COLOR_WINDOWTEXT),
       }});
}

void AddMixerToMapToCrossPlatformIds(ColorProvider* provider) {
  // TODO(pkasting): Add recipes
}

void AddNativeColorMixers(ColorProvider* provider) {
  AddMixerForNativeColors(provider);
  AddMixerToMapToCrossPlatformIds(provider);
}

}  // namespace ui
