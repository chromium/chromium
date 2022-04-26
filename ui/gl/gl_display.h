// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_DISPLAY_H_
#define UI_GL_GL_DISPLAY_H_

#include "ui/gl/gl_export.h"

#if defined(USE_EGL)
#include <EGL/egl.h>
#endif  // defined(USE_EGL)

namespace gl {

class EGLDisplayPlatform {
 public:
  constexpr EGLDisplayPlatform()
      : display_(EGL_DEFAULT_DISPLAY), platform_(0), valid_(false) {}
  explicit constexpr EGLDisplayPlatform(EGLNativeDisplayType display,
                                        int platform = 0)
      : display_(display), platform_(platform), valid_(true) {}

  bool Valid() const { return valid_; }
  int GetPlatform() const { return platform_; }
  EGLNativeDisplayType GetDisplay() const { return display_; }

 private:
  EGLNativeDisplayType display_;
  // 0 for default, or EGL_PLATFORM_* enum.
  int platform_;
  bool valid_;
};

// If adding a new type, also add it to EGLDisplayType in
// tools/metrics/histograms/enums.xml. Don't remove or reorder entries.
enum DisplayType {
  DEFAULT = 0,
  SWIFT_SHADER = 1,
  ANGLE_WARP = 2,
  ANGLE_D3D9 = 3,
  ANGLE_D3D11 = 4,
  ANGLE_OPENGL = 5,
  ANGLE_OPENGLES = 6,
  ANGLE_NULL = 7,
  ANGLE_D3D11_NULL = 8,
  ANGLE_OPENGL_NULL = 9,
  ANGLE_OPENGLES_NULL = 10,
  ANGLE_VULKAN = 11,
  ANGLE_VULKAN_NULL = 12,
  ANGLE_D3D11on12 = 13,
  ANGLE_SWIFTSHADER = 14,
  ANGLE_OPENGL_EGL = 15,
  ANGLE_OPENGLES_EGL = 16,
  ANGLE_METAL = 17,
  ANGLE_METAL_NULL = 18,
  DISPLAY_TYPE_MAX = 19,
};

class GL_EXPORT GLDisplay {
 public:
  GLDisplay();

  GLDisplay(const GLDisplay&) = delete;
  GLDisplay& operator=(const GLDisplay&) = delete;

  virtual ~GLDisplay();

  virtual void* GetDisplay() = 0;
};

#if defined(USE_EGL)
class GL_EXPORT GLDisplayEGL : public GLDisplay {
 public:
  GLDisplayEGL();
  explicit GLDisplayEGL(EGLDisplay display);

  GLDisplayEGL(const GLDisplayEGL&) = delete;
  GLDisplayEGL& operator=(const GLDisplayEGL&) = delete;

  ~GLDisplayEGL() override;

  EGLDisplay GetDisplay() override;
  void SetDisplay(EGLDisplay display);

  EGLDisplay GetHardwareDisplay();

  EGLNativeDisplayType GetNativeDisplay();
  DisplayType GetDisplayType();

  bool HasEGLClientExtension(const char* name);
  bool HasEGLExtension(const char* name);
  bool IsCreateContextRobustnessSupported();
  bool IsRobustnessVideoMemoryPurgeSupported();
  bool IsCreateContextBindGeneratesResourceSupported();
  bool IsCreateContextWebGLCompatabilitySupported();
  bool IsEGLSurfacelessContextSupported();
  bool IsEGLContextPrioritySupported();
  bool IsEGLNoConfigContextSupported();
  bool IsRobustResourceInitSupported();
  bool IsDisplayTextureShareGroupSupported();
  bool IsDisplaySemaphoreShareGroupSupported();
  bool IsCreateContextClientArraysSupported();
  bool IsAndroidNativeFenceSyncSupported();
  bool IsPixelFormatFloatSupported();
  bool IsANGLEFeatureControlSupported();
  bool IsANGLEPowerPreferenceSupported();
  bool IsANGLEDisplayPowerPreferenceSupported();
  bool IsANGLEPlatformANGLEDeviceIdSupported();
  bool IsANGLEExternalContextAndSurfaceSupported();
  bool IsANGLEContextVirtualizationSupported();
  bool IsANGLEVulkanImageSupported();
  bool IsEGLQueryDeviceSupported();

  EGLDisplayPlatform native_display = EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);

  DisplayType display_type = DisplayType::DEFAULT;

  const char* egl_client_extensions = nullptr;
  const char* egl_extensions = nullptr;
  bool egl_create_context_robustness_supported = false;
  bool egl_robustness_video_memory_purge_supported = false;
  bool egl_create_context_bind_generates_resource_supported = false;
  bool egl_create_context_webgl_compatability_supported = false;
  bool egl_sync_control_supported = false;
  bool egl_sync_control_rate_supported = false;
  bool egl_window_fixed_size_supported = false;
  bool egl_surfaceless_context_supported = false;
  bool egl_surface_orientation_supported = false;
  bool egl_context_priority_supported = false;
  bool egl_khr_colorspace = false;
  bool egl_ext_colorspace_display_p3 = false;
  bool egl_ext_colorspace_display_p3_passthrough = false;
  bool egl_no_config_context_supported = false;
  bool egl_robust_resource_init_supported = false;
  bool egl_display_texture_share_group_supported = false;
  bool egl_display_semaphore_share_group_supported = false;
  bool egl_create_context_client_arrays_supported = false;
  bool egl_android_native_fence_sync_supported = false;
  bool egl_ext_pixel_format_float_supported = false;
  bool egl_angle_feature_control_supported = false;
  bool egl_angle_power_preference_supported = false;
  bool egl_angle_display_power_preference_supported = false;
  bool egl_angle_platform_angle_device_id_supported = false;
  bool egl_angle_external_context_and_surface_supported = false;
  bool egl_ext_query_device_supported = false;
  bool egl_angle_context_virtualization_supported = false;
  bool egl_angle_vulkan_image_supported = false;

 private:
  EGLDisplay display_;
};
#endif  // defined(USE_EGL)

#if defined(USE_GLX)
class GL_EXPORT GLDisplayX11 : public GLDisplay {
 public:
  GLDisplayX11();

  GLDisplayX11(const GLDisplayX11&) = delete;
  GLDisplayX11& operator=(const GLDisplayX11&) = delete;

  ~GLDisplayX11() override;

  void* GetDisplay() override;
};
#endif  // defined(USE_GLX)

}  // namespace gl

#endif  // UI_GL_GL_DISPLAY_H_