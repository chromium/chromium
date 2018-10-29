// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ui/gl/gl_switches.h"

namespace gl {

const char kGLImplementationDesktopName[] = "desktop";
const char kGLImplementationAppleName[] = "apple";
const char kGLImplementationEGLName[] = "egl";
const char kGLImplementationANGLEName[] = "angle";
const char kGLImplementationSwiftShaderName[] = "swiftshader";
const char kGLImplementationSwiftShaderForWebGLName[] = "swiftshader-webgl";
const char kGLImplementationMockName[] = "mock";
const char kGLImplementationStubName[] = "stub";
const char kGLImplementationDisabledName[] = "disabled";

const char kANGLEImplementationDefaultName[]  = "default";
const char kANGLEImplementationD3D9Name[]     = "d3d9";
const char kANGLEImplementationD3D11Name[]    = "d3d11";
const char kANGLEImplementationOpenGLName[]   = "gl";
const char kANGLEImplementationOpenGLESName[] = "gles";
const char kANGLEImplementationNullName[] = "null";
const char kANGLEImplementationVulkanName[] = "vulkan";

// Special switches for "NULL"/stub driver implementations.
const char kANGLEImplementationD3D11NULLName[] = "d3d11-null";
const char kANGLEImplementationOpenGLNULLName[] = "gl-null";
const char kANGLEImplementationOpenGLESNULLName[] = "gles-null";
const char kANGLEImplementationVulkanNULLName[] = "vulkan-null";

}  // namespace gl

namespace switches {

// Disables use of D3D11.
const char kDisableD3D11[]                  = "disable-d3d11";

// Disables use of ES3 backend (use ES2 backend instead).
const char kDisableES3GLContext[]           = "disable-es3-gl-context";

// Stop the GPU from synchronizing presentation with vblank.
const char kDisableGpuVsync[]               = "disable-gpu-vsync";

// Turns on GPU logging (debug build only).
const char kEnableGPUServiceLogging[]       = "enable-gpu-service-logging";

// Turns on calling TRACE for every GL call.
const char kEnableGPUServiceTracing[]       = "enable-gpu-service-tracing";

// Select which ANGLE backend to use. Options are:
//  default: Attempts several ANGLE renderers until one successfully
//           initializes, varying ES support by platform.
//  d3d9: Legacy D3D9 renderer, ES2 only.
//  d3d11: D3D11 renderer, ES2 and ES3.
//  warp: D3D11 renderer using software rasterization, ES2 and ES3.
//  gl: Desktop GL renderer, ES2 and ES3.
//  gles: GLES renderer, ES2 and ES3.
const char kUseANGLE[]                      = "use-angle";

// Select which implementation of GL the GPU process should use. Options are:
//  desktop: whatever desktop OpenGL the user has installed (Linux and Mac
//           default).
//  egl: whatever EGL / GLES2 the user has installed (Windows default - actually
//       ANGLE).
//  swiftshader: The SwiftShader software renderer.
const char kUseGL[]                         = "use-gl";

// Inform Chrome that a GPU context will not be lost in power saving mode,
// screen saving mode, etc.  Note that this flag does not ensure that a GPU
// context will never be lost in any situations, say, a GPU reset.
const char kGpuNoContextLost[]              = "gpu-no-context-lost";

// Flag used for Linux tests: for desktop GL bindings, try to load this GL
// library first, but fall back to regular library if loading fails.
const char kTestGLLib[]                     = "test-gl-lib";

// Use hardware gpu, if available, for tests.
const char kUseGpuInTests[] = "use-gpu-in-tests";

// Enable use of the SGI_video_sync extension, which can have
// driver/sandbox/window manager compatibility issues.
const char kEnableSgiVideoSync[] = "enable-sgi-video-sync";

// Disables GL drawing operations which produce pixel output. With this
// the GL output will not be correct but tests will run faster.
const char kDisableGLDrawingForTests[] = "disable-gl-drawing-for-tests";

// Forces the use of software GL instead of hardware gpu.
const char kOverrideUseSoftwareGLForTests[] =
    "override-use-software-gl-for-tests";

// Disables specified comma separated GL Extensions if found.
const char kDisableGLExtensions[] = "disable-gl-extensions";

// Enables SwapBuffersWithBounds if it is supported.
const char kEnableSwapBuffersWithBounds[] = "enable-swap-buffers-with-bounds";

// Enables using DirectComposition layers, even if hardware overlays aren't
// supported.
const char kEnableDirectCompositionLayers[] =
    "enable-direct-composition-layers";

// Disables using DirectComposition layers.
const char kDisableDirectCompositionLayers[] =
    "disable-direct-composition-layers";

// This is the list of switches passed from this file that are passed from the
// GpuProcessHost to the GPU Process. Add your switch to this list if you need
// to read it in the GPU process, else don't add it.
const char* const kGLSwitchesCopiedFromGpuProcessHost[] = {
    kDisableGpuVsync,
    kDisableD3D11,
    kDisableES3GLContext,
    kEnableGPUServiceLogging,
    kEnableGPUServiceTracing,
    kEnableSgiVideoSync,
    kGpuNoContextLost,
    kDisableGLDrawingForTests,
    kOverrideUseSoftwareGLForTests,
    kUseANGLE,
    kEnableSwapBuffersWithBounds,
    kEnableDirectCompositionLayers,
    kDisableDirectCompositionLayers,
};
const int kGLSwitchesCopiedFromGpuProcessHostNumSwitches =
    arraysize(kGLSwitchesCopiedFromGpuProcessHost);

}  // namespace switches

namespace features {

// Allow putting a video swapchain underneath the main swapchain, so overlays
// can be used even if there are controls on top of the video. This requires
// the DirectCompositionOverlays feature to be enabled.
const base::Feature kDirectCompositionUnderlays{
    "DirectCompositionUnderlays", base::FEATURE_DISABLED_BY_DEFAULT};

// Allow putting content with complex transforms (e.g. rotations) into an
// overlay.
const base::Feature kDirectCompositionComplexOverlays{
    "DirectCompositionComplexOverlays", base::FEATURE_DISABLED_BY_DEFAULT};

// Allow using overlays for non-root render passes.
const base::Feature kDirectCompositionNonrootOverlays{
    "DirectCompositionNonrootOverlays", base::FEATURE_DISABLED_BY_DEFAULT};

// Default to using ANGLE's OpenGL backend
const base::Feature kDefaultANGLEOpenGL{"DefaultANGLEOpenGL",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
