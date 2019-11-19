// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_android.h"

#include "base/logging.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {
// These are the default dimensions of radio buttons and checkboxes on Android.
const int kCheckboxAndRadioWidth = 16;
const int kCheckboxAndRadioHeight = 16;
}

#if !defined(USE_AURA)
// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeAndroid::instance();
}

NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  NOTREACHED();
  return nullptr;
}
#endif

// static
NativeThemeAndroid* NativeThemeAndroid::instance() {
  static base::NoDestructor<NativeThemeAndroid> s_native_theme;
  return s_native_theme.get();
}

gfx::Size NativeThemeAndroid::GetPartSize(Part part,
                                          State state,
                                          const ExtraParams& extra) const {
  if (part == kCheckbox || part == kRadio)
    return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
  return NativeThemeBase::GetPartSize(part, state, extra);
}

SkColor NativeThemeAndroid::GetSystemColor(ColorId color_id,
                                           ColorScheme color_scheme) const {
  NOTIMPLEMENTED();
  return SK_ColorBLACK;
}

void NativeThemeAndroid::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // Take 1px for padding around the checkbox/radio button.
  rect->setLTRB(static_cast<int>(rect->x()) + 1,
                static_cast<int>(rect->y()) + 1,
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

NativeThemeAndroid::NativeThemeAndroid() {
}

NativeThemeAndroid::~NativeThemeAndroid() {
}

}  // namespace ui
