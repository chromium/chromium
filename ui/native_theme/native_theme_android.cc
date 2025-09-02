// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_android.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeAndroid::instance();
}

NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeAndroid::instance();
}

// static
NativeThemeAndroid* NativeThemeAndroid::instance() {
  static base::NoDestructor<NativeThemeAndroid> s_native_theme;
  return s_native_theme.get();
}

gfx::Size NativeThemeAndroid::GetPartSize(Part part,
                                          State state,
                                          const ExtraParams& extra) const {
  if (part == kCheckbox || part == kRadio) {
    // Define the dimensions of radio buttons and checkboxes on Android. They
    // are slightly bigger than the defaults in native_theme_base.cc, to make
    // touch easier on small form factor devices.
    static constexpr gfx::Size kCheckboxAndRadioSize(16, 16);
    return kCheckboxAndRadioSize;
  }
  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeAndroid::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // Take 1px for padding around the checkbox/radio button.
  rect->setLTRB(static_cast<int>(rect->x()) + 1,
                static_cast<int>(rect->y()) + 1,
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

NativeThemeAndroid::NativeThemeAndroid() = default;

NativeThemeAndroid::~NativeThemeAndroid() = default;

}  // namespace ui
