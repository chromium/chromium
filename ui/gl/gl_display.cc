// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display.h"
#include "base/notreached.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_GLX)
#include "ui/gl/glx_util.h"
#endif  // defined(USE_GLX)

namespace gl {

GLDisplay::GLDisplay() = default;

GLDisplay::~GLDisplay() = default;

#if defined(USE_EGL)
GLDisplayEGL::GLDisplayEGL() {
  display_ = EGL_NO_DISPLAY;
}

GLDisplayEGL::GLDisplayEGL(EGLDisplay display) {
  display_ = display;
}

GLDisplayEGL::~GLDisplayEGL() = default;

EGLDisplay GLDisplayEGL::GetDisplay() {
  return display_;
}

void GLDisplayEGL::SetDisplay(EGLDisplay display) {
  display_ = display;
}

EGLDisplay GLDisplayEGL::GetHardwareDisplay() {
  return GetDisplay();
}

EGLNativeDisplayType GLDisplayEGL::GetNativeDisplay() {
  return native_display.GetDisplay();
}

DisplayType GLDisplayEGL::GetDisplayType() {
  return display_type;
}

bool GLDisplayEGL::HasEGLClientExtension(const char* name) {
  if (!egl_client_extensions)
    return false;
  return GLSurface::ExtensionsContain(egl_client_extensions, name);
}

bool GLDisplayEGL::HasEGLExtension(const char* name) {
  return GLSurface::ExtensionsContain(egl_extensions, name);
}

bool GLDisplayEGL::IsCreateContextRobustnessSupported() {
  return egl_create_context_robustness_supported;
}

bool GLDisplayEGL::IsRobustnessVideoMemoryPurgeSupported() {
  return egl_robustness_video_memory_purge_supported;
}

bool GLDisplayEGL::IsCreateContextBindGeneratesResourceSupported() {
  return egl_create_context_bind_generates_resource_supported;
}

bool GLDisplayEGL::IsCreateContextWebGLCompatabilitySupported() {
  return egl_create_context_webgl_compatability_supported;
}

bool GLDisplayEGL::IsEGLSurfacelessContextSupported() {
  return egl_surfaceless_context_supported;
}

bool GLDisplayEGL::IsEGLContextPrioritySupported() {
  return egl_context_priority_supported;
}

bool GLDisplayEGL::IsEGLNoConfigContextSupported() {
  return egl_no_config_context_supported;
}

bool GLDisplayEGL::IsRobustResourceInitSupported() {
  return egl_robust_resource_init_supported;
}

bool GLDisplayEGL::IsDisplayTextureShareGroupSupported() {
  return egl_display_texture_share_group_supported;
}

bool GLDisplayEGL::IsDisplaySemaphoreShareGroupSupported() {
  return egl_display_semaphore_share_group_supported;
}

bool GLDisplayEGL::IsCreateContextClientArraysSupported() {
  return egl_create_context_client_arrays_supported;
}

bool GLDisplayEGL::IsAndroidNativeFenceSyncSupported() {
  return egl_android_native_fence_sync_supported;
}

bool GLDisplayEGL::IsPixelFormatFloatSupported() {
  return egl_ext_pixel_format_float_supported;
}

bool GLDisplayEGL::IsANGLEFeatureControlSupported() {
  return egl_angle_feature_control_supported;
}

bool GLDisplayEGL::IsANGLEPowerPreferenceSupported() {
  return egl_angle_power_preference_supported;
}

bool GLDisplayEGL::IsANGLEDisplayPowerPreferenceSupported() {
  return egl_angle_display_power_preference_supported;
}

bool GLDisplayEGL::IsANGLEPlatformANGLEDeviceIdSupported() {
  return egl_angle_platform_angle_device_id_supported;
}

bool GLDisplayEGL::IsANGLEExternalContextAndSurfaceSupported() {
  return egl_angle_external_context_and_surface_supported;
}

bool GLDisplayEGL::IsANGLEContextVirtualizationSupported() {
  return egl_angle_context_virtualization_supported;
}

bool GLDisplayEGL::IsANGLEVulkanImageSupported() {
  return egl_angle_vulkan_image_supported;
}

bool GLDisplayEGL::IsEGLQueryDeviceSupported() {
  return egl_ext_query_device_supported;
}

#endif  // defined(USE_EGL)

#if defined(USE_GLX)
GLDisplayX11::GLDisplayX11() = default;

GLDisplayX11::~GLDisplayX11() = default;

void* GLDisplayX11::GetDisplay() {
  return x11::Connection::Get()->GetXlibDisplay();
}
#endif  // defined(USE_GLX)

}  // namespace gl