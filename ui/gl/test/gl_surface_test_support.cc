// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_surface_test_support.h"

#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/platform_window/common/platform_window_defaults.h"  // nogncheck
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gl {

namespace {

GLDisplay* InitializeOneOffHelper(bool init_extensions) {
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::test::EnableTestConfigForPlatformWindows();
#endif

  bool use_software_gl = true;

  // We usually use software GL as this works on all bots. The
  // command line can override this behaviour to use hardware GL.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    use_software_gl = false;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On Android and iOS we always use hardware GL.
  use_software_gl = false;
#endif

  std::vector<GLImplementationParts> allowed_impls =
      init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());

  GLImplementationParts impl = allowed_impls[0];
  if (use_software_gl) {
    impl = gl::GetSoftwareGLImplementation();
  }

  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL))
      << "kUseGL has not effect in tests";

  bool disable_gl_drawing = true;

  CHECK(gl::init::InitializeStaticGLBindingsImplementation(impl));
  GLDisplay* display = gl::init::InitializeGLOneOffPlatformImplementation(
      disable_gl_drawing, init_extensions, gl::GpuPreference::kDefault);
  CHECK(display);
  return display;
}
}  // namespace

// static
GLDisplay* GLSurfaceTestSupport::InitializeOneOff() {
  return InitializeOneOffHelper(true);
}

// static
GLDisplay* GLSurfaceTestSupport::InitializeNoExtensionsOneOff() {
  return InitializeOneOffHelper(false);
}

// static
GLDisplay* GLSurfaceTestSupport::InitializeOneOffImplementation(
    GLImplementationParts impl) {
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL))
      << "kUseGL has not effect in tests";

  bool disable_gl_drawing = false;
  bool init_extensions = true;

  CHECK(gl::init::InitializeStaticGLBindingsImplementation(impl));
  GLDisplay* display = gl::init::InitializeGLOneOffPlatformImplementation(
      disable_gl_drawing, init_extensions, gl::GpuPreference::kDefault);
  CHECK(display);
  return display;
}

// static
GLDisplay* GLSurfaceTestSupport::InitializeOneOffWithMockBindings() {
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  return InitializeOneOffImplementation(
      GLImplementationParts(kGLImplementationMockGL));
}

// static
GLDisplay* GLSurfaceTestSupport::InitializeOneOffWithStubBindings() {
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif
  return InitializeOneOffImplementation(
      GLImplementationParts(kGLImplementationStubGL));
}

// static
GLDisplay* GLSurfaceTestSupport::InitializeOneOffWithNullAngleBindings() {
#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif
  auto* display = InitializeOneOffImplementation(
      GLImplementationParts(gl::ANGLEImplementation::kNull));

  DCHECK_EQ(gl::GetANGLEImplementation(), gl::ANGLEImplementation::kNull);
  return display;
}

// static
void GLSurfaceTestSupport::ShutdownGL(GLDisplay* display) {
  init::ShutdownGL(display, false);
}

}  // namespace gl
