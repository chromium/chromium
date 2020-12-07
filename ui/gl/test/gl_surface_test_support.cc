// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_surface_test_support.h"

#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "ui/platform_window/common/platform_window_defaults.h"  // nogncheck
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#endif

#if defined(USE_X11) || defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

namespace gl {

namespace {
void InitializeOneOffHelper(bool init_extensions) {
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForGPU(params);
  }
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  ui::test::EnableTestConfigForPlatformWindows();
#endif

  bool use_software_gl = true;

  // We usually use software GL as this works on all bots. The
  // command line can override this behaviour to use hardware GL.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    use_software_gl = false;
  }

#if defined(OS_ANDROID)
  // On Android we always use hardware GL.
  use_software_gl = false;
#endif

  std::vector<GLImplementation> allowed_impls =
      init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());

  GLImplementation impl = allowed_impls[0];
  if (use_software_gl) {
    impl = gl::GetSoftwareGLImplementation();

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
#if defined(USE_OZONE)
    if (!features::IsUsingOzonePlatform())
#endif
    {
      // If ANGLE is available use it with SwiftShader Vulkan instead of using
      // SwiftShader GL
      for (auto i : allowed_impls) {
        if (i == kGLImplementationEGLANGLE) {
          impl = kGLImplementationEGLANGLE;
          base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
              switches::kUseANGLE, kANGLEImplementationSwiftShaderName);
          base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
              switches::kUseCmdDecoder, kCmdDecoderValidatingName);
          break;
        }
      }
    }
#endif
  }

  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL))
      << "kUseGL has not effect in tests";

  bool fallback_to_software_gl = false;
  bool disable_gl_drawing = true;

  CHECK(gl::init::InitializeStaticGLBindingsImplementation(
      impl, fallback_to_software_gl));
  CHECK(gl::init::InitializeGLOneOffPlatformImplementation(
      fallback_to_software_gl, disable_gl_drawing, init_extensions));
}
}  // namespace

// static
void GLSurfaceTestSupport::InitializeOneOff() {
  InitializeOneOffHelper(true);
}

// static
void GLSurfaceTestSupport::InitializeNoExtensionsOneOff() {
  InitializeOneOffHelper(false);
}

// static
void GLSurfaceTestSupport::InitializeOneOffImplementation(
    GLImplementation impl,
    bool fallback_to_software_gl) {
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL))
      << "kUseGL has not effect in tests";

  // This method may be called multiple times in the same process to set up
  // bindings in different ways.
  init::ShutdownGL(false);

  bool disable_gl_drawing = false;

  CHECK(gl::init::InitializeStaticGLBindingsImplementation(
      impl, fallback_to_software_gl));
  CHECK(gl::init::InitializeGLOneOffPlatformImplementation(
      fallback_to_software_gl, disable_gl_drawing, true));
}

// static
void GLSurfaceTestSupport::InitializeOneOffWithMockBindings() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForGPU(params);
  }
#endif

  InitializeOneOffImplementation(kGLImplementationMockGL, false);
}

// static
void GLSurfaceTestSupport::InitializeOneOffWithStubBindings() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForGPU(params);
  }
#endif

  InitializeOneOffImplementation(kGLImplementationStubGL, false);
}

// static
void GLSurfaceTestSupport::ShutdownGL() {
  init::ShutdownGL(false);
}

}  // namespace gl
