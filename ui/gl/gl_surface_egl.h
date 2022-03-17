// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_H_
#define UI_GL_GL_SURFACE_EGL_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/egl_timestamps.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_overlay.h"

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

class GLSurfacePresentationHelper;

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

GL_EXPORT void GetEGLInitDisplays(bool supports_angle_d3d,
                                  bool supports_angle_opengl,
                                  bool supports_angle_null,
                                  bool supports_angle_vulkan,
                                  bool supports_angle_swiftshader,
                                  bool supports_angle_egl,
                                  bool supports_angle_metal,
                                  const base::CommandLine* command_line,
                                  std::vector<DisplayType>* init_displays);

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  GLSurfaceEGL();

  GLSurfaceEGL(const GLSurfaceEGL&) = delete;
  GLSurfaceEGL& operator=(const GLSurfaceEGL&) = delete;
  virtual EGLint GetNativeVisualID() const;

  // Implement GLSurface.
  EGLDisplay GetDisplay() override;
  EGLConfig GetConfig() override;
  GLSurfaceFormat GetFormat() override;

  // |system_device_id| specifies which GPU to use on a multi-GPU system.
  // If its value is 0, use the default GPU of the system.
  static bool InitializeOneOff(EGLDisplayPlatform native_display,
                               uint64_t system_device_id);
  static bool InitializeOneOffForTesting();
  static bool InitializeExtensionSettingsOneOff();
  static void ShutdownOneOff();
  static EGLDisplay GetHardwareDisplay();
  // |system_device_id| specifies which GPU to use on a multi-GPU system.
  // If its value is 0, use the default GPU of the system.
  static EGLDisplay InitializeDisplay(EGLDisplayPlatform native_display,
                                      uint64_t system_device_id);
  static EGLNativeDisplayType GetNativeDisplay();
  static DisplayType GetDisplayType();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetEGLClientExtensions();
  static const char* GetEGLExtensions();
  static bool HasEGLClientExtension(const char* name);
  static bool HasEGLExtension(const char* name);
  static bool IsCreateContextRobustnessSupported();
  static bool IsRobustnessVideoMemoryPurgeSupported();
  static bool IsCreateContextBindGeneratesResourceSupported();
  static bool IsCreateContextWebGLCompatabilitySupported();
  static bool IsEGLSurfacelessContextSupported();
  static bool IsEGLContextPrioritySupported();
  static bool IsEGLNoConfigContextSupported();
  static bool IsRobustResourceInitSupported();
  static bool IsDisplayTextureShareGroupSupported();
  static bool IsDisplaySemaphoreShareGroupSupported();
  static bool IsCreateContextClientArraysSupported();
  static bool IsAndroidNativeFenceSyncSupported();
  static bool IsPixelFormatFloatSupported();
  static bool IsANGLEFeatureControlSupported();
  static bool IsANGLEPowerPreferenceSupported();
  static bool IsANGLEDisplayPowerPreferenceSupported();
  static bool IsANGLEPlatformANGLEDeviceIdSupported();
  static bool IsANGLEExternalContextAndSurfaceSupported();
  static bool IsANGLEContextVirtualizationSupported();
  static bool IsANGLEVulkanImageSupported();
  static bool IsEGLQueryDeviceSupported();

 protected:
  ~GLSurfaceEGL() override;

  EGLConfig config_ = nullptr;
  GLSurfaceFormat format_;

 private:
  static bool InitializeOneOffCommon();
  static bool initialized_;
};

// Encapsulates an EGL surface bound to a view.
class GL_EXPORT NativeViewGLSurfaceEGL : public GLSurfaceEGL,
                                         public EGLTimestampClient {
 public:
  NativeViewGLSurfaceEGL(EGLNativeWindowType window,
                         std::unique_ptr<gfx::VSyncProvider> vsync_provider);

  NativeViewGLSurfaceEGL(const NativeViewGLSurfaceEGL&) = delete;
  NativeViewGLSurfaceEGL& operator=(const NativeViewGLSurfaceEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  bool SupportsSwapTimestamps() const override;
  void SetEnableSwapTimestamps() override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool Recreate() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  EGLSurface GetHandle() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
  bool SupportsCommitOverlayPlanes() override;
  gfx::SwapResult CommitOverlayPlanes(PresentationCallback callback) override;
  bool OnMakeCurrent(GLContext* context) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  void SetVSyncEnabled(bool enabled) override;
  bool ScheduleOverlayPlane(
      GLImage* image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;
  gfx::SurfaceOrigin GetOrigin() const override;
  EGLTimestampClient* GetEGLTimestampClient() override;

  // EGLTimestampClient implementation.
  bool IsEGLTimestampSupported() const override;

  bool GetFrameTimestampInfoIfAvailable(base::TimeTicks* presentation_time,
                                        base::TimeDelta* composite_interval,
                                        base::TimeTicks* writes_done_time,
                                        uint32_t* presentation_flags,
                                        int frame_id) override;

  // Takes care of the platform dependant bits, of any, for creating the window.
  virtual bool InitializeNativeWindow();

 protected:
  ~NativeViewGLSurfaceEGL() override;

  EGLNativeWindowType window_ = 0;
  gfx::Size size_ = gfx::Size(1, 1);
  bool enable_fixed_size_angle_ = true;

  GLSurfacePresentationHelper* presentation_helper() const {
    return presentation_helper_.get();
  }

  gfx::SwapResult SwapBuffersWithDamage(const std::vector<int>& rects,
                                        PresentationCallback callback);

 private:
  struct SwapInfo {
    bool frame_id_is_valid;
    EGLuint64KHR frame_id;
  };

  void UpdateSwapEvents(EGLuint64KHR newFrameId, bool newFrameIdIsValid);
  void TraceSwapEvents(EGLuint64KHR oldFrameId);

  // Some platforms may provide a custom implementation of vsync provider.
  virtual std::unique_ptr<gfx::VSyncProvider> CreateVsyncProviderInternal();

  EGLSurface surface_ = nullptr;
  bool supports_post_sub_buffer_ = false;
  bool supports_swap_buffer_with_damage_ = false;
  gfx::SurfaceOrigin surface_origin_ = gfx::SurfaceOrigin::kBottomLeft;

  std::unique_ptr<gfx::VSyncProvider> vsync_provider_external_;
  std::unique_ptr<gfx::VSyncProvider> vsync_provider_internal_;

  // Stored in separate vectors so we can pass the egl timestamps
  // directly to the EGL functions.
  bool use_egl_timestamps_ = false;
  std::vector<EGLint> supported_egl_timestamps_;
  std::vector<const char*> supported_event_names_;

  // PresentationFeedback support.
  int presentation_feedback_index_ = -1;
  int composition_start_index_ = -1;
  int writes_done_index_ = -1;
  uint32_t presentation_flags_ = 0;

  base::queue<SwapInfo> swap_info_queue_;

  bool vsync_enabled_ = true;
  std::unique_ptr<GLSurfacePresentationHelper> presentation_helper_;
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  explicit PbufferGLSurfaceEGL(const gfx::Size& size);

  PbufferGLSurfaceEGL(const PbufferGLSurfaceEGL&) = delete;
  PbufferGLSurfaceEGL& operator=(const PbufferGLSurfaceEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~PbufferGLSurfaceEGL() override;

 private:
  gfx::Size size_;
  EGLSurface surface_;
};

// SurfacelessEGL is used as Offscreen surface when platform supports
// KHR_surfaceless_context and GL_OES_surfaceless_context. This would avoid the
// need to create a dummy EGLsurface in case we render to client API targets.
class GL_EXPORT SurfacelessEGL : public GLSurfaceEGL {
 public:
  explicit SurfacelessEGL(const gfx::Size& size);

  SurfacelessEGL(const SurfacelessEGL&) = delete;
  SurfacelessEGL& operator=(const SurfacelessEGL&) = delete;

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  bool IsSurfaceless() const override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLSurface GetHandle() override;
  void* GetShareHandle() override;

 protected:
  ~SurfacelessEGL() override;

 private:
  gfx::Size size_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_H_
