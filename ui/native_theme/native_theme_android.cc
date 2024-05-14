// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_android.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
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
  NOTREACHED_IN_MIGRATION();
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

void NativeThemeAndroid::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // Take 1px for padding around the checkbox/radio button.
  rect->setLTRB(static_cast<int>(rect->x()) + 1,
                static_cast<int>(rect->y()) + 1,
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

SkColor NativeThemeAndroid::ControlsAccentColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kPressed) {
    color_id = kPressedAccent;
  } else if (state == kDisabled) {
    color_id = kDisabledAccent;
  } else {
    color_id = kAccent;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeAndroid::ControlsSliderColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kPressed) {
    color_id = kPressedSlider;
  } else if (state == kDisabled) {
    color_id = kDisabledSlider;
  } else {
    color_id = kSlider;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeAndroid::ControlsBorderColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kPressed) {
    color_id = kPressedBorder;
  } else if (state == kDisabled) {
    color_id = kDisabledBorder;
  } else {
    color_id = kBorder;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeAndroid::ButtonBorderColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kPressed) {
    color_id = kButtonPressedBorder;
  } else if (state == kDisabled) {
    color_id = kButtonDisabledBorder;
  } else {
    color_id = kButtonBorder;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeAndroid::ControlsFillColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kPressed) {
    color_id = kPressedFill;
  } else if (state == kDisabled) {
    color_id = kDisabledFill;
  } else {
    color_id = kFill;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

SkColor NativeThemeAndroid::ButtonFillColorForState(
    State state,
    ColorScheme color_scheme,
    const ColorProvider* color_provider) const {
  ControlColorId color_id;
  if (state == kPressed) {
    color_id = kButtonPressedFill;
  } else if (state == kDisabled) {
    color_id = kButtonDisabledFill;
  } else {
    color_id = kButtonFill;
  }
  return GetControlColor(color_id, color_scheme, color_provider);
}

NativeThemeAndroid::NativeThemeAndroid() {
}

NativeThemeAndroid::~NativeThemeAndroid() {
}

}  // namespace ui
