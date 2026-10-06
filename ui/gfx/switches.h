// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SWITCHES_H_
#define UI_GFX_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/gfx/switches_export.h"

namespace switches {

GFX_SWITCHES_EXPORT extern const char kAnimationDurationScale[];
GFX_SWITCHES_EXPORT extern const char kDisableFontSubpixelPositioning[];
GFX_SWITCHES_EXPORT extern const char kEnableNativeGpuMemoryBuffers[];
GFX_SWITCHES_EXPORT extern const char kForcePrefersReducedMotion[];
GFX_SWITCHES_EXPORT extern const char kForcePrefersNoReducedMotion[];
GFX_SWITCHES_EXPORT extern const char kHeadless[];
GFX_SWITCHES_EXPORT extern const char kScreenInfo[];

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
GFX_SWITCHES_EXPORT extern const char kX11Display[];
GFX_SWITCHES_EXPORT extern const char kNoXshm[];
#endif

}  // namespace switches

namespace features {

GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kUseSmartRefForGPUFenceHandle);
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kUseRoundedPointConversion);
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kHdrAgtmParseOldSyntax);

// Workaround for an issue in Windows where icons with fully transparent
// pixels are rendered as black squares. See https://crbug.com/441293180
// Used as a killswitch in case an issue is discovered with the implementation.
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kTransparentIconWorkaround);

// When enabled, HarfBuzz uses SkTypeface::copyTableData to load font tables.
// On platforms that support zero-copy table access (DirectWrite on Windows,
// CoreText on macOS), this avoids allocating a buffer and copying the table
// data.
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kHarfBuzzZeroCopyFontTable);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// When enabled, fontconfig initialization is done asynchronously on a worker
// thread to prevent blocking browser startup. See https://crbug.com/41496758.
GFX_SWITCHES_EXPORT BASE_DECLARE_FEATURE(kAsyncFontconfigInitialization);
#endif

}  // namespace features

#endif  // UI_GFX_SWITCHES_H_
