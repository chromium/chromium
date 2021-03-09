// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/mac/system_color_utils.h"

#import <Cocoa/Cocoa.h>

#include "third_party/skia/include/core/SkScalar.h"

namespace {
bool graphite_tint_test_override = false;
}

namespace ui {

bool IsSystemGraphiteTinted() {
  if (graphite_tint_test_override)
    return true;

  return [NSColor currentControlTint] == NSGraphiteControlTint;
}

SkColor ColorToGrayscale(SkColor color) {
  SkScalar luminance = SkColorGetR(color) * 0.21 + SkColorGetG(color) * 0.72 +
                       SkColorGetB(color) * 0.07;
  uint8_t component = SkScalarRoundToInt(luminance);
  return SkColorSetARGB(SkColorGetA(color), component, component, component);
}

ScopedEnableGraphiteTint::ScopedEnableGraphiteTint() {
  original_test_override_ = graphite_tint_test_override;
  graphite_tint_test_override = true;
}

ScopedEnableGraphiteTint::~ScopedEnableGraphiteTint() {
  graphite_tint_test_override = original_test_override_;
}

}  // ui
