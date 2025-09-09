// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mobile.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

gfx::Size NativeThemeMobile::GetPartSize(Part part,
                                         State state,
                                         const ExtraParams& extra) const {
  if (part == kCheckbox || part == kRadio) {
    // Radio buttons and checkboxes are slightly bigger than the defaults in
    // `NativeThemeBase`, to make touch easier on small form factor devices.
    static constexpr gfx::Size kCheckboxAndRadioSize(16, 16);
    return kCheckboxAndRadioSize;
  }
  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeMobile::AdjustCheckboxRadioRectForPadding(SkRect* rect) const {
  // Take 1px for padding around the checkbox/radio button.
  rect->setLTRB(static_cast<int>(rect->x()) + 1,
                static_cast<int>(rect->y()) + 1,
                static_cast<int>(rect->right()) - 1,
                static_cast<int>(rect->bottom()) - 1);
}

NativeThemeMobile::NativeThemeMobile() = default;

NativeThemeMobile::~NativeThemeMobile() = default;

}  // namespace ui
