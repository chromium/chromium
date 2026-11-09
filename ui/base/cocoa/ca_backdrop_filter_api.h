// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_CA_BACKDROP_FILTER_API_H_
#define UI_BASE_COCOA_CA_BACKDROP_FILTER_API_H_

#include <array>

#include "base/component_export.h"
#include "base/feature_list.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#endif  // __OBJC__

namespace ui {

// Feature controlling CoreAnimation hardware-accelerated CSS backdrop filter.
COMPONENT_EXPORT(UI_BASE) BASE_DECLARE_FEATURE(kCoreAnimationBackdropFilter);

// Returns true if CoreAnimation backdrop filter (CABackdropLayer and CAFilter)
// is supported and enabled.
bool COMPONENT_EXPORT(UI_BASE) CABackdropLayerSupported();

#if defined(__OBJC__)
// Creates a CABackdropLayer instance. Only call when
// CABackdropLayerSupported() is true.
CALayer* COMPONENT_EXPORT(UI_BASE) CreateCABackdropLayer();

// Creates a CAFilter configured for Gaussian blur (`blur_radius` > 0 and finite
// in points, `inputNormalizeEdges` enabled). Only call when
// CABackdropLayerSupported() is true.
id COMPONENT_EXPORT(UI_BASE) CreateCABlurFilter(double blur_radius);

// Creates a CAFilter configured with a 4x5 row-major `colorMatrix`. Only call
// when CABackdropLayerSupported() is true.
id COMPONENT_EXPORT(UI_BASE)
    CreateCAColorMatrixFilter(const std::array<float, 20>& matrix);
#endif  // __OBJC__

}  // namespace ui

#endif  // UI_BASE_COCOA_CA_BACKDROP_FILTER_API_H_
