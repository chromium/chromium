// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/mac/scoped_current_nsappearance.h"

#import <Cocoa/Cocoa.h>

namespace ui {
ScopedCurrentNSAppearance::ScopedCurrentNSAppearance(bool dark,
                                                     bool high_contrast) {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearance;

    if (dark) {
      appearance = high_contrast
                       ? NSAppearanceNameAccessibilityHighContrastDarkAqua
                       : NSAppearanceNameDarkAqua;
    } else {
      appearance = high_contrast ? NSAppearanceNameAccessibilityHighContrastAqua
                                 : NSAppearanceNameAqua;
    }

    [NSAppearance
        setCurrentAppearance:[NSAppearance appearanceNamed:appearance]];
  }
}

ScopedCurrentNSAppearance::~ScopedCurrentNSAppearance() {
  if (@available(macOS 10.14, *))
    [NSAppearance setCurrentAppearance:nil];
}
}