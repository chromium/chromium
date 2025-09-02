// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_ios.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {
// These are the default dimensions of radio buttons and checkboxes on Android.
const int kCheckboxAndRadioWidth = 16;
const int kCheckboxAndRadioHeight = 16;
}  // namespace

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeIOS::instance();
}

NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  NOTREACHED();
}

// static
NativeThemeIOS* NativeThemeIOS::instance() {
  static base::NoDestructor<NativeThemeIOS> s_native_theme;
  return s_native_theme.get();
}

gfx::Size NativeThemeIOS::GetPartSize(Part part,
                                      State state,
                                      const ExtraParams& extra) const {
  if (part == kCheckbox || part == kRadio) {
    return gfx::Size(kCheckboxAndRadioWidth, kCheckboxAndRadioHeight);
  }
  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeIOS::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // Take 1px for padding around the checkbox/radio button.
  rect->setLTRB(static_cast<int>(rect->x()) + 1,
                static_cast<int>(rect->y()) + 1,
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

NativeThemeIOS::NativeThemeIOS() = default;

NativeThemeIOS::~NativeThemeIOS() = default;

}  // namespace ui
