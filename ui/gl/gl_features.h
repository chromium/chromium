// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FEATURES_H_
#define UI_GL_GL_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "ui/gl/gl_export.h"

namespace features {

// Controls if GPU should synchronize presentation with vsync.
GL_EXPORT bool UseGpuVsync();

#if BUILDFLAG(IS_ANDROID)
// Use new Android 13 API to obtain and target a frame deadline.
GL_EXPORT BASE_DECLARE_FEATURE(kAndroidFrameDeadline);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) ||     \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define PASSTHROUGH_COMMAND_DECODER_LAUNCHED
#else
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GL_EXPORT BASE_DECLARE_FEATURE(kDefaultPassthroughCommandDecoder);
#endif

GL_EXPORT bool IsAndroidFrameDeadlineEnabled();

GL_EXPORT bool UsePassthroughCommandDecoder();

}  // namespace features

#endif  // UI_GL_GL_FEATURES_H_
