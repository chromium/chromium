// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWITCHES_H_
#define UI_GFX_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/switches_export.h"

namespace switches {

GFX_SWITCHES_EXPORT extern const char kAnimationDurationScale[];
GFX_SWITCHES_EXPORT extern const char kDisableFontSubpixelPositioning[];
GFX_SWITCHES_EXPORT extern const char kEnableNativeGpuMemoryBuffers[];
GFX_SWITCHES_EXPORT extern const char kForcePrefersReducedMotion[];
GFX_SWITCHES_EXPORT extern const char kForcePrefersNoReducedMotion[];
GFX_SWITCHES_EXPORT extern const char kHeadless[];

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
GFX_SWITCHES_EXPORT extern const char kX11Display[];
GFX_SWITCHES_EXPORT extern const char kNoXshm[];
#endif

}  // namespace switches

namespace features {

GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kOddHeightMultiPlanarBuffers);
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kOddWidthMultiPlanarBuffers);
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kUseSmartRefForGPUFenceHandle);

}  // namespace features

#endif  // UI_GFX_SWITCHES_H_
