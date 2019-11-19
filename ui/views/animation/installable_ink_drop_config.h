// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_CONFIG_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_CONFIG_H_

#include "third_party/skia/include/core/SkColor.h"

namespace views {

struct InstallableInkDropConfig {
  // The color of ink drop effects, modulated by opacity.
  SkColor base_color;
  // The opacity to paint |base_color| at for a fully-visible ripple.
  float ripple_opacity;
  // The opacity to paint |base_color| at for a fully-visible hover highlight.
  float highlight_opacity;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_CONFIG_H_
