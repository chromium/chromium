// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_support.h"

#include <vector>

#include "base/check_op.h"
#include "build/build_config.h"
#include "gpu/config/gpu_util.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_OZONE)
#include "base/run_loop.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gl {

// static
GLDisplay* GLTestSupport::InitializeGL(
    std::optional<GLImplementationParts> prefered_impl) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  gpu::TrySetNonSoftwareDevicePreferenceForTesting(
      gl::GpuPreference ::kDefault);
#endif

#if BUILDFLAG(IS_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  std::vector<GLImplementationParts> allowed_impls =
      init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());

  GLImplementationParts impl =
      prefered_impl ? *prefered_impl : allowed_impls[0];
  DCHECK(impl.IsAllowed(allowed_impls));

  GLDisplay* display =
      GLSurfaceTestSupport::InitializeOneOffImplementation(impl);
#if BUILDFLAG(IS_OZONE)
  // Make sure all the tasks posted to the current task runner by the
  // initialization functions are run before running the tests.
  base::RunLoop().RunUntilIdle();
#endif
  return display;
}

// static
void GLTestSupport::CleanupGL(GLDisplay* display) {
  GLSurfaceTestSupport::ShutdownGL(display);
}

}  // namespace gl
