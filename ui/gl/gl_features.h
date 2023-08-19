// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FEATURES_H_
#define UI_GL_GL_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_export.h"

namespace features {

// Controls if GPU should synchronize presentation with vsync.
GL_EXPORT bool UseGpuVsync();

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GL_EXPORT BASE_DECLARE_FEATURE(kDefaultPassthroughCommandDecoder);
#endif

#if BUILDFLAG(IS_MAC)
GL_EXPORT BASE_DECLARE_FEATURE(kWriteMetalShaderCacheToDisk);
GL_EXPORT BASE_DECLARE_FEATURE(kUseBuiltInMetalShaderCache);
#endif

#if BUILDFLAG(IS_WIN)
GL_EXPORT BASE_DECLARE_FEATURE(kUsePrimaryMonitorVSyncIntervalOnSV3);
#endif  // BUILDFLAG(IS_WIN)

GL_EXPORT bool IsAndroidFrameDeadlineEnabled();

GL_EXPORT bool UsePassthroughCommandDecoder();

}  // namespace features

#endif  // UI_GL_GL_FEATURES_H_
