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
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
GL_EXPORT BASE_DECLARE_FEATURE(kFallbackToSWIfGLES3NotSupported);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
GL_EXPORT BASE_DECLARE_FEATURE(kUseCompositorClockVSyncInterval);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
// Controls whether 2-pixel even boundary alignment is enforced for YUV 4:2:0
// and 4:2:2 SurfaceControl overlays to prevent odd-coordinate hardware scaler
// rejections.
GL_EXPORT BASE_DECLARE_FEATURE(kAndroidYuvOverlayEvenAlignment);
#endif  // BUILDFLAG(IS_ANDROID)

GL_EXPORT bool IsAndroidFrameDeadlineEnabled();

GL_EXPORT bool UsePassthroughCommandDecoder();
GL_EXPORT bool IsANGLEValidationEnabled();
GL_EXPORT bool IsANGLEPassthroughShadersAllowed();

GL_EXPORT void GetANGLEFeaturesFromCommandLineAndFinch(
    const base::CommandLine* command_line,
    std::vector<std::string>& enabled_angle_features,
    std::vector<std::string>& disabled_angle_features);

GL_EXPORT bool ShouldFallbackToSWIfGLES3NotSupported();

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

// If SwiftShader is explicitly requested for WebGL via
// --use-angle=swiftshader-webgl.
GL_EXPORT bool IsSwiftShaderUsedForWebGLByCommandLine(
    const base::CommandLine* command_line);

#if BUILDFLAG(IS_WIN)
GL_EXPORT BASE_DECLARE_FEATURE(kAllowD3D11WarpFallback);

GL_EXPORT BASE_DECLARE_FEATURE(kDCompOnD3D12);
#endif

GL_EXPORT bool IsWARPAllowed(const base::CommandLine* command_line);

// Check if any form of software WebGL fallback is available
GL_EXPORT bool IsAnySoftwareGLAllowed(const base::CommandLine* command_line);

// Check if falling back to software GL due to crashes on hardware GL is
// allowed.
GL_EXPORT bool IsSoftwareGLFallbackDueToCrashesAllowed(
    const base::CommandLine* command_line);

#if BUILDFLAG(IS_ANDROID)
GL_EXPORT BASE_DECLARE_FEATURE(kAndroidLimitRgb565DisplayToApi32);
GL_EXPORT BASE_DECLARE_FEATURE(kAndroidSurfaceControlPartialDamage);

GL_EXPORT bool PreferRGB565ResourcesForDisplay();
#endif

}  // namespace features

#endif  // UI_GL_GL_FEATURES_H_
