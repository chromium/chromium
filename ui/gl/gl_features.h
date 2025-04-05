// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FEATURES_H_
#define UI_GL_GL_FEATURES_H_

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_export.h"

namespace features {

// Controls if GPU should synchronize presentation with vsync.
GL_EXPORT bool UseGpuVsync();

// Controls if vsync interval should be based on compositor clock.
GL_EXPORT bool UseCompositorClockVSyncInterval();

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GL_EXPORT BASE_DECLARE_FEATURE(kDefaultPassthroughCommandDecoder);
GL_EXPORT BASE_DECLARE_FEATURE(kAddDelayToGLCompileShader);
#endif

#if BUILDFLAG(IS_WIN)
GL_EXPORT BASE_DECLARE_FEATURE(kUsePrimaryMonitorVSyncIntervalOnSV3);
GL_EXPORT BASE_DECLARE_FEATURE(kUseCompositorClockVSyncInterval);
#endif  // BUILDFLAG(IS_WIN)

GL_EXPORT bool IsAndroidFrameDeadlineEnabled();

GL_EXPORT bool UsePassthroughCommandDecoder();
GL_EXPORT bool IsANGLEValidationEnabled();
GL_EXPORT bool IsANGLEPassthroughShadersAllowed();

GL_EXPORT void GetANGLEFeaturesFromCommandLineAndFinch(
    const base::CommandLine* command_line,
    std::vector<std::string>& enabled_angle_features,
    std::vector<std::string>& disabled_angle_features);

#if BUILDFLAG(ENABLE_SWIFTSHADER)
GL_EXPORT BASE_DECLARE_FEATURE(kAllowSwiftShaderFallback);
#endif

// If SwiftShader should be allowed as a GL implementation or WebGL fallback via
// command line flags. Disallowed by default unless explicitly requested with
// --use-angle=swiftshader[-for-webgl], --enable-unsafe-swiftshader
GL_EXPORT bool IsSwiftShaderAllowedByCommandLine(
    const base::CommandLine* command_line);

// If SwiftShader should be allowed due to the AllowSwiftShaderFallback
// killswitch feature.
GL_EXPORT bool IsSwiftShaderAllowedByFeature();

// SwiftShader is allowed by either IsSwiftShaderAllowedByCommandLine or
// IsSwiftShaderAllowedByFeature.
GL_EXPORT bool IsSwiftShaderAllowed(const base::CommandLine* command_line);

#if BUILDFLAG(IS_WIN)
GL_EXPORT BASE_DECLARE_FEATURE(kAllowD3D11WarpFallback);
#endif

// Check if any form of software WebGL fallback is available
GL_EXPORT bool IsAnySoftwareGLAllowed(const base::CommandLine* command_line);

// Check if falling back to software GL due to crashes on hardware GL is
// allowed.
GL_EXPORT bool IsSoftwareGLFallbackDueToCrashesAllowed(
    const base::CommandLine* command_line);

// Query the delay we add to glCompileShader.
// Default is 0 if kAddDelayToGLCompileShader is off.
GL_EXPORT base::TimeDelta GetGLCompileShaderDelay();
}  // namespace features

#endif  // UI_GL_GL_FEATURES_H_
